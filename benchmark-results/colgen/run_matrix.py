"""Runs the measurement matrix, one subprocess per run, resumable (see plans/colgen/measurements.md).

  python run_matrix.py --output benchmark-results/colgen/small [--instances A,B] [--configs a,b] [--repeats N]
Ctrl-C loses only the run in progress; an existing output folder resumes.
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

CONFIGS = {"example-mip": ["example"], "example-cpp": ["cpp"]}
for _loop in ("cpp", "python"):
    for _master in ("highs", "highspy", "gurobipy"):
        for _pricing in ("exact", "heuristic"):
            CONFIGS[f"layer-{_loop}-{_master}-{_pricing}"] = [
                "layer", "--loop", _loop, "--master", _master, "--pricing", _pricing,
            ]

FIELDS = [
    "instance", "config", "repeat", "exit_code", "status", "lp", "iterations", "columns",
    "wall_s", "cpu_user_s", "cpu_sys_s", "peak_mem_mb", "master_s", "pricing_s", "rest_s", "integer_s",
]


def run_one(instance, config, log_path):
    command = [sys.executable, os.path.join(HERE, "drivers.py"), *CONFIGS[config], "--instance", instance]
    row = {}
    start = time.perf_counter()
    peak, user, system = 0, 0.0, 0.0
    with open(log_path, "w") as log:
        child = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        process = psutil.Process(child.pid)
        while child.poll() is None:
            mem, cpu_user, cpu_system = 0, 0.0, 0.0
            try:
                # the venv's python.exe is a launcher: measure the whole process tree
                for member in [process] + process.children(recursive=True):
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
            time.sleep(0.2)
    row["wall_s"] = time.perf_counter() - start
    row.update(exit_code=child.returncode, cpu_user_s=user, cpu_sys_s=system, peak_mem_mb=peak / 2**20)
    with open(log_path) as log:
        for line in reversed(log.read().splitlines()):
            if line.startswith("RESULT "):
                row.update(json.loads(line[7:]))
                break
    if "status" not in row:
        row["status"] = "failed"
    return row


def sample_machine(path, stop):
    tracked = {}
    with open(path, "a", newline="") as handle:
        writer = csv.writer(handle)
        if handle.tell() == 0:
            writer.writerow(["time", "cpu_percent", "memory_percent", "busiest_others"])
        while not stop.wait(5.0):
            load = []
            for process in psutil.process_iter(["name"]):
                if process.pid == os.getpid():
                    continue
                known = tracked.setdefault(process.pid, process)
                try:
                    load.append((known.cpu_percent(None), process.info["name"]))
                except psutil.Error:
                    pass
            top = "; ".join(f"{name} {cpu:.0f}%" for cpu, name in sorted(load, reverse=True)[:3])
            writer.writerow([time.strftime("%H:%M:%S"), psutil.cpu_percent(), psutil.virtual_memory().percent, top])
            handle.flush()


def read_rows(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


def number(value, spec=".2f"):
    try:
        return format(float(value), spec)
    except (TypeError, ValueError):
        return "-"


def write_summary(output, rows):
    lines = ["# Summary", ""]
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
        lines += ["| config | rep | status | LP | iters | columns | wall s | master s | pricing s | rest s | peak MB |",
                  "|---|---|---|---|---|---|---|---|---|---|---|"]
        for r in group:
            lines.append("| " + " | ".join([
                r["config"], r["repeat"], r["status"], number(r["lp"], ".4f"), r["iterations"] or "-",
                r["columns"] or "-", number(r["wall_s"]), number(r["master_s"]), number(r["pricing_s"]),
                number(r["rest_s"]), number(r["peak_mem_mb"], ".0f")]) + " |")
        lines.append("")
    lines += ["## By configuration (mean wall s)", ""]
    instances = list(by_instance)
    lines += ["| config | " + " | ".join(instances) + " |", "|---|" + "---|" * len(instances)]
    for config in CONFIGS:
        cells = []
        for instance in instances:
            walls = [float(r["wall_s"]) for r in by_instance[instance] if r["config"] == config]
            cells.append(number(sum(walls) / len(walls)) if walls else "-")
        if any(c != "-" for c in cells):
            lines.append(f"| {config} | " + " | ".join(cells) + " |")
    lines += ["", "## LP mismatches", ""] + (mismatches or ["none"])
    with open(os.path.join(output, "summary.md"), "w") as handle:
        handle.write("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--instances", help="comma list, or 'small' / 'large'; default small")
    parser.add_argument("--configs", help="comma list; default all")
    parser.add_argument("--repeats", type=int, default=1)
    args = parser.parse_args()

    instances = {"small": SMALL, "large": LARGE, None: SMALL}.get(args.instances) or args.instances.split(",")
    configs = args.configs.split(",") if args.configs else list(CONFIGS)
    unknown = [c for c in configs if c not in CONFIGS]
    if unknown:
        sys.exit(f"unknown configs {unknown}; known: {list(CONFIGS)}")

    output = args.output
    os.makedirs(os.path.join(output, "logs"), exist_ok=True)
    results = os.path.join(output, "results.csv")
    rows = read_rows(results)
    wanted = [(i, c, str(k)) for k in range(args.repeats) for i in instances for c in configs]
    done = {(r["instance"], r["config"], r["repeat"]) for r in rows}
    todo = [key for key in wanted if key not in done]
    total = len(wanted)
    finished = total - len(todo)

    stop = threading.Event()
    threading.Thread(target=sample_machine, args=(os.path.join(output, "machine.csv"), stop), daemon=True).start()
    durations = []
    try:
        for instance, config, repeat in todo:
            finished += 1
            status = f"[{finished}/{total}] {instance} {config} rep {repeat}"
            if durations:
                eta = sum(durations) / len(durations) * (total - finished + 1)
                status += f" (ETA {eta / 60:.0f} min)"
            print(status + " ...", flush=True)
            with open(os.path.join(output, "status.txt"), "w") as handle:
                handle.write(status + f"\nstarted {time.strftime('%H:%M:%S')}\n")
            row = run_one(instance, config, os.path.join(output, "logs", f"{instance}-{config}-{repeat}.log"))
            row.update(instance=instance, config=config, repeat=repeat)
            durations.append(row["wall_s"])
            fresh = not os.path.exists(results)
            with open(results, "a", newline="") as handle:
                writer = csv.DictWriter(handle, FIELDS, extrasaction="ignore")
                if fresh:
                    writer.writeheader()
                writer.writerow(row)
            rows.append({k: str(row.get(k, "")) for k in FIELDS})
            print(f"    {row['status']} lp={number(row.get('lp'), '.4f')} wall={row['wall_s']:.1f}s", flush=True)
            write_summary(output, rows)
    finally:
        stop.set()
    with open(os.path.join(output, "status.txt"), "w") as handle:
        handle.write(f"finished {time.strftime('%H:%M:%S')}\n")


if __name__ == "__main__":
    main()
