"""The numbers of the theory pages (docs/theory/), computed with rcspp.

Every page of the theory series works through a small instance. Each module here recomputes one
page's numbers: ``numbers()`` returns them, ``report()`` formats them. Run them all with
``python -m theory`` from ``examples/python``, or one page with ``python -m theory --page 3``.
``tests/python/test_theory_numbers.py`` checks them against the values the pages state.
"""
