#!/usr/bin/env python3
"""C++ coverage gate: overall line coverage >= threshold and no large uncovered function.

Usage (after running gcovr with json output):
    python3 scripts/check_cpp_coverage.py [--json PATH] [--min-lines N] [--threshold PCT]

Defaults:
    --json       coverage/cpp.json
    --min-lines  10   (functions with fewer instrumented lines are ignored)
    --threshold  85   (minimum overall line-coverage %)

Exits 0 on pass, 1 on failure. Compatible with gcovr 7+ JSON schema (field "file").

When gcovr is run with --merge-mode-functions=separate (the default) it can emit
multiple entries for the same source file (one per template-instantiation TU).
This script merges them: a line is "covered" if any instantiation covered it,
and a function is "uncovered" only if *all* instantiations have execution_count=0.
"""

import argparse
import json
import sys


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--json", default="coverage/cpp.json", help="gcovr JSON report path")
    parser.add_argument(
        "--min-lines",
        type=int,
        default=10,
        help="ignore uncovered functions with fewer instrumented lines than this",
    )
    parser.add_argument(
        "--threshold",
        type=float,
        default=85.0,
        help="minimum overall line-coverage percentage required",
    )
    args = parser.parse_args()

    min_fn_lines: int = args.min_lines
    line_threshold: float = args.threshold

    with open(args.json) as f:
        data = json.load(f)

    # gcovr 7+ uses "file"; older versions used "filename" — accept both.
    def _fname(file_data: dict) -> str:
        return file_data.get("file") or file_data.get("filename", "<unknown>")

    # ── Merge all entries for the same source file ─────────────────────────────
    # gcovr may emit multiple entries per source file when template functions are
    # instantiated in different TUs (merge-mode-functions=separate).  We take the
    # max hit-count per line (covered if ANY instantiation covered it) and the max
    # execution_count per function (uncovered only if ALL instantiations have 0).
    file_lines: dict[str, dict[int, int]] = {}
    # fname -> {(name, start_line): fn_dict}
    file_fns: dict[str, dict[tuple, dict]] = {}

    for file_data in data.get("files", []):
        fname = _fname(file_data)

        if fname not in file_lines:
            file_lines[fname] = {}
        for line in file_data.get("lines", []):
            ln = line["line_number"]
            cnt = line.get("count", 0)
            file_lines[fname][ln] = max(file_lines[fname].get(ln, 0), cnt)

        if fname not in file_fns:
            file_fns[fname] = {}
        for fn in file_data.get("functions", []):
            key = (fn.get("name", ""), fn.get("start_line", 0))
            existing = file_fns[fname].get(key)
            if existing is None or fn.get("execution_count", 0) > existing.get("execution_count", 0):
                file_fns[fname][key] = fn

    # ── Gate 1: overall line coverage ─────────────────────────────────────────
    total_lines = covered_lines = 0
    for lines in file_lines.values():
        total_lines += len(lines)
        covered_lines += sum(1 for h in lines.values() if h > 0)

    pct = covered_lines / total_lines * 100 if total_lines else 0.0

    print("\n=== C++ line coverage by file ===")
    for fname, lines in sorted(file_lines.items()):
        uncov = sorted(ln for ln, h in lines.items() if h == 0)
        if uncov:
            cov_pct = (len(lines) - len(uncov)) / len(lines) * 100
            tail = "..." if len(uncov) > 20 else ""
            print(f"  {fname}: {cov_pct:.1f}%  missing={uncov[:20]}{tail}")
    print(f"\nTotal: {covered_lines}/{total_lines} = {pct:.1f}%")

    # ── Gate 2: no large uncovered function ───────────────────────────────────
    large_uncovered: list[str] = []
    for fname, lines in file_lines.items():
        if not lines:
            continue
        fns = sorted(file_fns.get(fname, {}).values(), key=lambda f: f.get("start_line", 0))
        for i, fn in enumerate(fns):
            if fn.get("execution_count", 0) != 0:
                continue
            start = fn.get("start_line", 0)
            # Skip function records with start_line=0: these are GCC/gcovr artifacts
            # for template lambda instantiations whose debug info has no reliable line
            # attribution.  Counting their "range" from line 0 to the next function
            # would span the entire file and produce false large-uncovered reports.
            if start == 0:
                continue
            end = (
                fns[i + 1].get("start_line", max(lines) + 1) - 1
                if i + 1 < len(fns)
                else max(lines, default=start)
            )
            fn_line_count = sum(1 for ln in lines if start <= ln <= end)
            if fn_line_count >= min_fn_lines:
                large_uncovered.append(
                    f"  {fname}: '{fn.get('name', '?')}' "
                    f"lines {start}-{end} ({fn_line_count} instrumented lines, 0 calls)"
                )

    failed = False
    if pct < line_threshold:
        print(f"\nFAIL: line coverage {pct:.1f}% < required {line_threshold}%")
        failed = True
    if large_uncovered:
        print(
            f"\nFAIL: {len(large_uncovered)} function(s) with >={min_fn_lines} "
            f"instrumented lines are completely uncovered:"
        )
        for msg in large_uncovered:
            print(msg)
        failed = True
    if not failed:
        print(f"\nPASS: {pct:.1f}% line coverage, no large uncovered functions.")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
