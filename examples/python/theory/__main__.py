"""Print the numbers of the theory pages: ``python -m theory [--page N]``."""

import argparse
import importlib
import sys

PAGES = (1, 2, 3, 4, 5, 6)


def main() -> None:
    """Print the report of every page, or of the one asked for."""
    sys.stdout.reconfigure(encoding="utf-8")  # the reports write 0′ and arrows
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--page", type=int, choices=PAGES, help="only this page")
    args = parser.parse_args()
    for page in [args.page] if args.page else PAGES:
        module = importlib.import_module(f"theory.page{page}")
        print(module.report())
        print()


if __name__ == "__main__":
    main()
