"""Print the numbers of the theory pages: ``python -m theory [--page N | --case NAME]``."""

import argparse
import importlib
import sys

PAGES = (1, 2, 3, 4, 5, 6)
CASE_STUDIES = ("crew",)


def main() -> None:
    """Print the report of every page and case study, or of the one asked for."""
    sys.stdout.reconfigure(encoding="utf-8")  # the reports write 0′ and arrows
    parser = argparse.ArgumentParser(description=__doc__)
    which = parser.add_mutually_exclusive_group()
    which.add_argument("--page", type=int, choices=PAGES, help="only this page")
    which.add_argument("--case", choices=CASE_STUDIES, help="only this case study")
    args = parser.parse_args()
    if args.page:
        modules = [f"page{args.page}"]
    elif args.case:
        modules = [args.case]
    else:
        modules = [f"page{page}" for page in PAGES] + list(CASE_STUDIES)
    for name in modules:
        print(importlib.import_module(f"theory.{name}").report())
        print()


if __name__ == "__main__":
    main()
