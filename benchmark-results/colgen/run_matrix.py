"""Runs the measurement matrix, one subprocess per run, resumable (see plans/colgen/measurements.md).

  python run_matrix.py --output benchmark-results/colgen/small [--instances A,B] [--configs a,b] [--repeats N]
  python run_matrix.py --output benchmark-results/colgen/small --summary-only

Ctrl-C loses only the run in progress; an existing output folder resumes. To stop cleanly after the run in
progress, create a file named STOP in the output folder. status.txt shows the run in progress, refreshed every
few seconds.
"""

import argparse
import csv
import json
import os
import subprocess
import sys
import threading
import time

import psutil

HERE = os.path.dirname(os.path.abspath(__file__))
SMALL = ["C101_25", "R101_25", "R201_25", "RC201_12", "C101_50", "R101_50", "RC101_50"]
LARGE = ["C101", "R101", "RC101", "C201", "R201", "RC201"]
BASELINE = {"C101": 828.94, "R101": 1636.39}
NOISY_CPU = 50.0  # other processes' mean CPU (in % of one core) above which a run's timing is flagged

CONFIGS = {"example-mip": ["example"], "example-cpp": ["cpp"]}
for _loop in ("cpp", "python"):
    for _master in ("highs", "highspy", "gurobipy"):
        for _pricing in ("exact", "heuristic"):
            CONFIGS[f"layer-{_loop}-{_master}-{_pricing}"] = [
                "layer", "--loop", _loop, "--master", _master, "--pricing", _pricing,
            ]

# The pure-Python layer (rcspp.python_column_generation): no C++ HiGHS master there, so highspy and gurobipy only.
for _master in ("highspy", "gurobipy"):
    for _pricing in ("exact", "heuristic", "tabu"):
        CONFIGS[f"layer-purepython-{_master}-{_pricing}"] = [
            "layer", "--loop", "purepython", "--master", _master, "--pricing", _pricing,
        ]
    CONFIGS[f"layer-cpp-{_master}-tabu"] = ["layer", "--loop", "cpp", "--master", _master, "--pricing", "tabu"]

FIELDS = [
    "instance", "config", "repeat", "exit_code", "status", "lp", "iterations", "columns",
    "wall_s", "solve_s", "cpu_user_s", "cpu_sys_s", "peak_mem_mb", "master_s", "pricing_s", "rest_s", "integer_s",
    "other_cpu_mean", "other_cpu_max",
]

# What the machine sampler shares with the runs: the pids of the run in progress, and (time, other CPU %) samples.
tree_pids = set()
samples = []
samples_lock = threading.Lock()


def solve_of(row):
    """The solve time of a row: the driver's own, else the sum of its parts."""
    try:
        return float(row["solve_s"])
    except (KeyError, TypeError, ValueError):
        pass
    parts = [row.get(k) for k in ("master_s", "pricing_s", "rest_s", "integer_s")]
    try:
        return sum(float(p) for p in parts if p not in (None, ""))
    except ValueError:
        return None


def run_one(instance, config, log_path, on_tick):
    command = [sys.executable, os.path.join(HERE, "drivers.py"), *CONFIGS[config], "--instance", instance]
    row = {}
    start = time.perf_counter()
    started_at = time.time()
    peak, user, system = 0, 0.0, 0.0
    last_tick = 0.0
    with open(log_path, "w") as log:
        child = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        process = psutil.Process(child.pid)
        while child.poll() is None:
            mem, cpu_user, cpu_system = 0, 0.0, 0.0
            try:
                # the venv's python.exe is a launcher: measure the whole process tree
                members = [process] + process.children(recursive=True)
                tree_pids.clear()
                tree_pids.update(m.pid for m in members)
                for member in members:
                    try:
                        info = member.memory_info()
                        mem += getattr(info, "peak_wset", info.rss)
                        times = member.cpu_times()
                        cpu_user += times.user
                        cpu_system += times.system
                    except psutil.Error:
                        pass
            except psutil.Error:
                pass
            peak, user, system = max(peak, mem), max(user, cpu_user), max(system, cpu_system)
            if time.perf_counter() - last_tick >= 5.0:
                last_tick = time.perf_counter()
                on_tick(time.perf_counter() - start, peak / 2**20)
            time.sleep(0.2)
    tree_pids.clear()
    row["wall_s"] = time.perf_counter() - start
    row.update(exit_code=child.returncode, cpu_user_s=user, cpu_sys_s=system, peak_mem_mb=peak / 2**20)
    with samples_lock:
        others = [cpu for when, cpu in samples if when >= started_at]
    if others:
        row.update(other_cpu_mean=sum(others) / len(others), other_cpu_max=max(others))
    read_result(log_path, row)
    if "status" not in row:
        row["status"] = "failed"
    row["solve_s"] = solve_of(row)
    return row


def read_result(log_path, row):
    """Adds the driver's `RESULT <json>` line of a log to `row`."""
    try:
        with open(log_path) as log:
            for line in reversed(log.read().splitlines()):
                if line.startswith("RESULT "):
                    row.update(json.loads(line[7:]))
                    return
    except OSError:
        pass


def sample_machine(path, stop):
    tracked = {}
    with open(path, "a", newline="") as handle:
        writer = csv.writer(handle)
        if handle.tell() == 0:
            writer.writerow(["time", "cpu_percent", "memory_percent", "other_cpu", "busiest_others"])
        while not stop.wait(5.0):
            load = []
            for process in psutil.process_iter(["name"]):
                if process.pid in (0, os.getpid()) or process.pid in tree_pids:
                    continue  # the idle process, this runner and the run in progress are not "others"
                known = tracked.setdefault(process.pid, process)
                try:
                    load.append((known.cpu_percent(None), process.info["name"]))
                except psutil.Error:
                    pass
            other = sum(cpu for cpu, _ in load)
            with samples_lock:
                samples.append((time.time(), other))
            top = "; ".join(f"{name} {cpu:.0f}%" for cpu, name in sorted(load, reverse=True)[:3])
            writer.writerow([time.strftime("%H:%M:%S"), psutil.cpu_percent(), psutil.virtual_memory().percent,
                             f"{other:.0f}", top])
            handle.flush()


def read_rows(path, output):
    """The rows of results.csv, migrated to the current columns first (old runs: solve time from their logs)."""
    if not os.path.exists(path):
        return []
    with open(path, newline="") as handle:
        reader = csv.DictReader(handle)
        old_fields, rows = reader.fieldnames, list(reader)
    if old_fields != FIELDS:
        for row in rows:
            if not row.get("solve_s"):
                log = os.path.join(output, "logs", f"{row['instance']}-{row['config']}-{row['repeat']}.log")
                from_log = {}
                read_result(log, from_log)
                value = from_log.get("solve_s", solve_of(row))
                row["solve_s"] = "" if value is None else value
        with open(path, "w", newline="") as handle:
            writer = csv.DictWriter(handle, FIELDS, extrasaction="ignore", restval="")
            writer.writeheader()
            writer.writerows(rows)
    return rows


def number(value, spec=".2f"):
    try:
        return format(float(value), spec)
    except (TypeError, ValueError):
        return "-"


def noisy(row):
    try:
        return float(row.get("other_cpu_mean")) > NOISY_CPU
    except (TypeError, ValueError):
        return False


def write_summary(output, rows):
    lines = ["# Summary", "",
             f"`*` after a time: other processes used more than {NOISY_CPU:.0f}% of a core on average",
             "during the run (see machine.csv), so the timing may be inflated.",
             "Solve = master + pricing + rest, without Python start-up.", ""]
    by_instance = {}
    for row in rows:
        by_instance.setdefault(row["instance"], []).append(row)
    mismatches = []
    for instance, group in by_instance.items():
        lps = []
        for r in group:
            try:
                lps.append(float(r["lp"]))
            except (TypeError, ValueError):
                pass
        lps = [v for v in lps if v == v]
        if lps and max(lps) - min(lps) > 1e-4 * max(1.0, abs(max(lps))):
            mismatches.append(f"{instance}: LP from {min(lps):.4f} to {max(lps):.4f}")
        lines += [f"## {instance}", ""]
        if instance in BASELINE:
            lines += [f"Baseline LP (current C++ example): {BASELINE[instance]}", ""]
        lines += ["| config | rep | status | LP | iters | columns | wall s | solve s | master s | pricing s | rest s | peak MB |",
                  "|---|---|---|---|---|---|---|---|---|---|---|---|"]
        for r in group:
            flag = "*" if noisy(r) else ""
            lines.append("| " + " | ".join([
                r["config"], r["repeat"], r["status"], number(r["lp"], ".4f"), r["iterations"] or "-",
                r["columns"] or "-", number(r["wall_s"]) + flag, number(solve_of(r)) + flag,
                number(r["master_s"]), number(r["pricing_s"]), number(r["rest_s"]),
                number(r["peak_mem_mb"], ".0f")]) + " |")
        lines.append("")
    instances = list(by_instance)
    for title, column in (("wall", lambda r: r["wall_s"]), ("solve", solve_of)):
        lines += [f"## By configuration (mean {title} s)", ""]
        lines += ["| config | " + " | ".join(instances) + " |", "|---|" + "---|" * len(instances)]
        for config in CONFIGS:
            cells = []
            for instance in instances:
                values = [column(r) for r in by_instance[instance] if r["config"] == config]
                values = [float(v) for v in values if v not in (None, "")]
                cells.append(number(sum(values) / len(values)) if values else "-")
            if any(c != "-" for c in cells):
                lines.append(f"| {config} | " + " | ".join(cells) + " |")
        lines.append("")
    lines += ["## LP mismatches", ""] + (mismatches or ["none"])
    with open(os.path.join(output, "summary.md"), "w") as handle:
        handle.write("\n".join(lines) + "\n")


def estimate(instance, config, rows):
    """Seconds a run should take, from a finished run of the same instance (any config), else None."""
    same = [float(r["wall_s"]) for r in rows if r["instance"] == instance and r["config"] == config]
    if not same:
        same = [float(r["wall_s"]) for r in rows if r["instance"] == instance]
    return sum(same) / len(same) if same else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--instances", help="comma list, or 'small' / 'large'; default small")
    parser.add_argument("--configs", help="comma list; default all")
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--summary-only", action="store_true", help="rewrite summary.md from results.csv and exit")
    args = parser.parse_args()

    output = args.output
    os.makedirs(os.path.join(output, "logs"), exist_ok=True)
    results = os.path.join(output, "results.csv")
    rows = read_rows(results, output)
    if args.summary_only:
        write_summary(output, rows)
        return

    instances = {"small": SMALL, "large": LARGE, None: SMALL}.get(args.instances) or args.instances.split(",")
    configs = args.configs.split(",") if args.configs else list(CONFIGS)
    unknown = [c for c in configs if c not in CONFIGS]
    if unknown:
        sys.exit(f"unknown configs {unknown}; known: {list(CONFIGS)}")

    wanted = [(i, c, str(k)) for k in range(args.repeats) for i in instances for c in configs]
    done = {(r["instance"], r["config"], r["repeat"]) for r in rows}
    todo = [key for key in wanted if key not in done]
    total = len(wanted)
    finished = total - len(todo)

    stop_file = os.path.join(output, "STOP")
    if os.path.exists(stop_file):
        os.remove(stop_file)  # a stale one from the last session
    stop = threading.Event()
    threading.Thread(target=sample_machine, args=(os.path.join(output, "machine.csv"), stop), daemon=True).start()
    status_path = os.path.join(output, "status.txt")
    stopped = False
    try:
        for index, (instance, config, repeat) in enumerate(todo):
            if os.path.exists(stop_file):
                stopped = True
                break
            finished += 1
            head = f"[{finished}/{total}] {instance} {config} rep {repeat}"
            known = [estimate(i, c, rows) for i, c, _ in todo[index + 1:]]
            eta = sum(e for e in known if e is not None)
            unknown_runs = sum(e is None for e in known)
            after = f"after it: about {eta / 60:.0f} min" + (f" + {unknown_runs} runs on instances not yet run" if unknown_runs else "")
            print(head + " ...", flush=True)
            started = time.strftime("%H:%M:%S")

            def on_tick(elapsed, peak_mb, head=head, after=after, started=started):
                with open(status_path, "w") as handle:
                    handle.write(f"{head}\nstarted {started}, running {elapsed / 60:.1f} min, peak {peak_mb:.0f} MB\n"
                                 f"{after}\ncreate a file STOP in this folder to stop after this run\n")

            on_tick(0.0, 0.0)
            row = run_one(instance, config, os.path.join(output, "logs", f"{instance}-{config}-{repeat}.log"), on_tick)
            row.update(instance=instance, config=config, repeat=repeat)
            fresh = not os.path.exists(results)
            with open(results, "a", newline="") as handle:
                writer = csv.DictWriter(handle, FIELDS, extrasaction="ignore", restval="")
                if fresh:
                    writer.writeheader()
                writer.writerow(row)
            rows.append({k: "" if row.get(k) is None else str(row.get(k)) for k in FIELDS})
            print(f"    {row['status']} lp={number(row.get('lp'), '.4f')} wall={row['wall_s']:.1f}s "
                  f"solve={number(row.get('solve_s'), '.1f')}s" + (" (noisy machine)" if noisy(row) else ""), flush=True)
            write_summary(output, rows)
    finally:
        stop.set()
    with open(status_path, "w") as handle:
        handle.write(f"{'stopped' if stopped else 'finished'} {time.strftime('%H:%M:%S')}\n")


if __name__ == "__main__":
    main()
