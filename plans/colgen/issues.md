# Issues to create on GitHub

Drafts only: nothing is filed until the user approves. Once filed, replace `draft` with `#number`. Labels are taken
from the repo's existing set. "Still on stacks" = also present at the tips of the open PR stacks (checked 2026-10-20).

| ID | title | labels | status | from |
|---|---|---|---|---|
| I-01 | `docs(solution): Solution.cost and Column.cost descriptions are swapped` | bug, documentation | #52 | `F-01`, `T-0.2` |
| I-02 | `refactor(graph): make Row::coefficient a double and reject non-finite coefficients` | enhancement | #53 | `F-12`, `T-0.4` |
| I-03 | `fix(benchmark): BKS table uses 1-decimal distances, so C101 reports a false FAIL` | bug | #54 | `F-11`, `T-0.5` |
| I-04 | `feat(logger): no ANSI colors when the output is not a terminal` | enhancement, good first issue | draft | `T-0.1b` |
| I-05 | `feat(benchmark): machine-readable results (CSV or JSON lines)` | enhancement | draft | `T-0.1b`, `T-2.5`, `Q-07` |
| I-06 | `question(vrp): default route relaxation of the VRP example` | question | #55 | `F-05`, `Q-09` |
| I-07 | `fix(ci): benchmark workflow can't build without Gurobi on ubuntu-latest` | bug | draft, inferred (never run); deferred (`D-52`) | `P-20` |
| I-08 | `feat: column generation layer (tracking issue)` | enhancement | after colleague review (`Q-12`) | this plan |

## Files

One file per issue in `issues/`, following the user's GitHub text rules (memory `feedback-github-text`): title as
the first `#` heading, no hard wraps inside paragraphs, Claude Code footer. Labels stay in the table above. Paste
the heading into the title field and the rest into the body. Checked on `main` @ `129b15e0`.

| ID | file |
|---|---|
| I-01 | `issues/I-01-solution-cost-docstrings.md` |
| I-02 | `issues/I-02-row-coefficient-double.md` |
| I-03 | `issues/I-03-benchmark-false-fail.md` |
| I-04 | `issues/I-04-logger-colors.md` |
| I-05 | `issues/I-05-benchmark-machine-readable.md` |
| I-06 | `issues/I-06-vrp-default-relaxation.md` |
| I-07 | `issues/I-07-benchmark-workflow-gurobi.md` |
| I-08 | not drafted: after colleague review (`Q-12`) |

## Existing issues closed

| # | title | status |
|---|---|---|
| #6 | Add tests for python | closed by the user 2026-10-21 ("Implemented."); work in #2, #13, #14, #17, #21, #23 |
| #7 | Create documentation and github pages | closed by the user 2026-10-21 ("Implemented."); work in #24, #25 |
