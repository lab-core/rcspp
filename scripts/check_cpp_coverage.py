#!/usr/bin/env python3
"""C++ coverage gate: overall line coverage >= threshold and no large uncovered function.

Usage (after running gcovr with json output):
    python3 scripts/check_cpp_coverage.py [--json PATH] [--min-lines N] [--threshold PCT]

Defaults:
    --json       coverage/cpp.json
    --min-lines  10   (functions with fewer instrumented lines are ignored)
    --threshold  85   (minimum overall line-coverage %)

Exits 0 on pass, 1 on failure. Compatible with gcovr 7+ JSON schema (field "file").
"""

import argparse
import json
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--json", default="coverage/cpp.json", help="gcovr JSON report path")
    parser.add_argument("--min-lines", type=int, default=10,
                        help="ignore uncovered functions with fewer instrumented lines than this")
    parser.add_argument("--threshold", type=float, default=85.0,
                        help="minimum overall line-coverage percentage required")
    args = parser.parse_args()

    min_fn_lines: int = args.min_lines
    line_threshold: float = args.threshold

    with open(args.json) as f:
        data = json.load(f)

    # gcovr 7+ uses "file"; older versions used "filename" — accept both.
    def _fname(file_data: dict) -> str:
        return file_data.get("file") or file_data.get("filename", "<unknown>")

    # ── Gate 1: overall line coverage ─────────────────────────────────────────
    total_lines = covered_lines = 0
    file_lines: dict[str, dict[int, int]] = {}
    for file_data in data.get("files", []):
        fname = _fname(file_data)
        lines = {line["line_number"]: line["count"] for line in file_data.get("lines", [])}
        file_lines[fname] = lines
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
    for file_data in data.get("files", []):
        fname = _fname(file_data)
        lines = file_lines[fname]
        if not lines:
            continue
        fns = sorted(file_data.get("functions", []), key=lambda f: f.get("start_line", 0))
        for i, fn in enumerate(fns):
            if fn.get("execution_count", 0) != 0:
                continue
            start = fn.get("start_line", 0)
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
