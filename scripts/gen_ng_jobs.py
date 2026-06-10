#!/usr/bin/env python3
"""Generate a job manifest for the ng-route benchmark SLURM array.

Each manifest line holds the CLI arguments for one ``rcspp-vrp-benchmark-ng``
run.  By default there is one job per (instance, ng-size) pair so the SLURM
array runs every instance in parallel; ``--group-by family`` instead emits one
job per (family, ng-size).  ``scripts/run_ng_benchmark.slurm`` reads the line
indexed by ``$SLURM_ARRAY_TASK_ID`` and runs it.

Example:
    python scripts/gen_ng_jobs.py --ng 3 5 8 --max-index 9
    sbatch --array=0-80 scripts/run_ng_benchmark.slurm
"""

from __future__ import annotations

import argparse
from pathlib import Path


def instance_names(families: list[str], max_index: int) -> list[str]:
    """Return Solomon instance names (e.g. C101) for the given families/indices.

    Args:
        families: Instance families to include (e.g. ["C", "R", "RC"]).
        max_index: Highest instance index; expands to <family>10<1..max_index>.

    Returns:
        The list of instance names in (index, family) order.
    """
    return [f"{family}10{i}" for i in range(1, max_index + 1) for family in families]


def build_jobs(args: argparse.Namespace) -> list[str]:
    """Build the manifest lines (one benchmark argument string per job).

    Args:
        args: Parsed command-line arguments.

    Returns:
        One argument string per job.
    """
    common: list[str] = ["--max-labels", str(args.max_labels)]
    if args.cols > 0:
        common += ["--cols", str(args.cols)]

    jobs: list[str] = []
    if args.group_by == "instance":
        for name in instance_names(args.families, args.max_index):
            for ng in args.ng:
                jobs.append(" ".join(["--instance", name, "--ng", str(ng), *common]))
    else:  # group_by == "family"
        for family in args.families:
            for ng in args.ng:
                jobs.append(
                    " ".join([str(args.max_index), "--family", family, "--ng", str(ng), *common])
                )
    return jobs


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        The parsed arguments.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--ng", type=int, nargs="+", default=[3, 5, 8], help="ng-neighbourhood sizes"
    )
    parser.add_argument(
        "--families",
        nargs="+",
        default=["C", "R", "RC"],
        help="Solomon instance families",
    )
    parser.add_argument("--max-index", type=int, default=9, help="highest instance index (1..9)")
    parser.add_argument("--max-labels", type=int, default=100, help="per-node label-expansion cap")
    parser.add_argument(
        "--cols", type=int, default=0, help="columns per CG iteration (0 => #customers)"
    )
    parser.add_argument(
        "--group-by",
        choices=["instance", "family"],
        default="instance",
        help="one job per instance (default) or per family",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("scripts/ng_jobs.txt"),
        help="manifest output path",
    )
    return parser.parse_args()


def main() -> None:
    """Generate the manifest and print the matching sbatch command."""
    args = parse_args()
    jobs = build_jobs(args)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(jobs) + "\n")

    print(f"Wrote {len(jobs)} jobs to {args.output}")
    print("Submit with:")
    print(
        f"  MANIFEST={args.output} sbatch --array=0-{len(jobs) - 1} "
        f"scripts/run_ng_benchmark.slurm"
    )


if __name__ == "__main__":
    main()
