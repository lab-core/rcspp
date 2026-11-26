# Issues to create on GitHub

Drafts only: nothing is filed until the user approves. Once filed, replace `draft` with `#number`. Labels are taken
from the repo's existing set. "Still on stacks" = also present at the tips of the open PR stacks (checked 2026-10-20).

| ID | title | labels | status | from |
|---|---|---|---|---|
| I-01 | `docs(solution): Solution.cost and Column.cost descriptions are swapped` | bug, documentation | #52 | `F-01`, `T-0.2` |
| I-02 | `refactor(graph): make Row::coefficient a double and reject non-finite coefficients` | enhancement | #53 | `F-12`, `T-0.4` |
| I-03 | `fix(benchmark): BKS table uses 1-decimal distances, so C101 reports a false FAIL` | bug | #54 | `F-11`, `T-0.5` |
| I-04 | `feat(logger): no ANSI colors when the output is not a terminal` | enhancement, good first issue | created: #105 (2026-11-23) | `T-0.1b` |
| I-05 | `feat(benchmark): machine-readable results (CSV or JSON lines)` | enhancement | closed 2026-11-23: the new benchmark writes JSON and CSV; #99 removes the old C++ benchmark | `T-0.1b`, `T-2.5`, `Q-07` |
| I-06 | `question(vrp): default route relaxation of the VRP example` | question | #55 | `F-05`, `Q-09` |
| I-07 | `fix(ci): benchmark workflow can't build without Gurobi on ubuntu-latest` | bug | closed 2026-11-23: #99 removes `benchmark.yml` | `P-20` |
| I-08 | `feat: column generation layer (tracking issue)` | enhancement | closed 2026-11-23: the PR stack tracks the work | this plan |
| I-09 | `docs(cpp): the "Choosing an algorithm" example of the C++ quickstart does not compile` | documentation | closed 2026-11-23: fixed on `main` (`LabelList<Resources>`) | — |
| I-10 | `fix(python): on Windows, the second Ctrl-C kills Python instead of raising KeyboardInterrupt` | bug | created: #104 (2026-11-23) | `F-31` |
| I-11 | `fix(build): cmake --install installs no rcspp headers` | bug | created: #79 (2026-10-30); no open PR fixes it, fixed in `T-1.12` (`D-107`) | `F-33` |
| I-12 | `docs: add rcspp.column_generation to the Python API reference` | documentation | created: #94 (2026-11-13); after the stack merges | `F-60` (2), `F-61` (3) |
| I-13 | `fix(algorithm): tabu search with default parameters logs an error and returns no path` | bug | created: #95 (2026-11-13) | `F-60` (1) |
| I-14 | `perf(examples): the C++ column generation example's column adds slow down as the master grows` | enhancement | created: #103 (2026-11-23) | `F-91`, `F-94` |
| I-15 | `perf(column-generation): column adds through gurobipy cost 20-80 % more per iteration than through python-mip` | enhancement | created: #101 (2026-11-23) | `F-94`, `F-47` |
| I-16 | `perf(python): the search called from Python runs 5-6 % slower than from C++ with tabu` | enhancement | created: #102 (2026-11-23) | `F-102` |
| I-17 | `perf(column-generation): the highspy master costs 1-2.6 ms more per iteration than the C++ example's HiGHS master` | enhancement | created: #100 (2026-11-23) | `F-101`, `F-47` |
| I-18 | `docs(python): ResourceGraph.solve's docstring omits the tabu search` | documentation | created: #106 (2026-11-23) | the roadmap pass's engine sweep |
| I-19 | `docs: pages that disagree with the code, and features with no docs` | documentation | created: #109 (2026-11-23; checked on `main` @ `ef4532d7`) | `T-1.21` (`features-and-options.md` K) |
| I-20 | `feat(python): a typed exception for a refused model; the resource functions Python cannot reach` | enhancement | created: #110 (2026-11-23; checked on `main` @ `ef4532d7`) | `T-1.21` (H.6; F.2-F.5, low priority) |
| I-60 | comment on #105: `the color reset is written after the line break` | — | draft (2026-11-26) | `F-160`, `F-111` |

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
| I-08 | not drafted: closed 2026-11-23 |
| I-09 | `issues/I-09-quickstart-choosing-an-algorithm.md` |
| I-10 | `issues/I-10-windows-second-ctrl-c.md` |
| I-11 | `issues/I-11-install-no-headers.md` |
| I-13 | `issues/I-13-tabu-default-max-iterations.md` |
| I-14 | `issues/I-14-cpp-example-column-adds.md` |
| I-15 | `issues/I-15-gurobipy-column-adds.md` |
| I-16 | `issues/I-16-search-from-python-slower.md` |
| I-17 | `issues/I-17-highspy-master-overhead.md` |
| I-18 | `issues/I-18-solve-docstring-tabu.md` |
| I-19 | `issues/I-19-docs-disagree-with-code.md` |
| I-20 | `issues/I-20-python-model-refused-and-functions.md` |
| I-60 | `issues/I-60-logger-reset-after-newline.md` |

Batch of 2026-11-23, created at once: #100-#106. Also: #99 removes
`benchmark_common.hpp`, so its description can say "Closes #54".

## Existing issues closed

| # | title | status |
|---|---|---|
| #6 | Add tests for python | closed by the user 2026-10-21 ("Implemented."); work in #2, #13, #14, #17, #21, #23 |
| #7 | Create documentation and github pages | closed by the user 2026-10-21 ("Implemented."); work in #24, #25 |
