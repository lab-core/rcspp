# Post-fold check, C101_25

The drivers run the folded stack (source checkout `../rcspp-dev`, #88's head `f31257f0`) through `run_matrix.ps1`.
Each run's LP value (to 1e-6), iterations and columns against the recorded results of the same configuration:

| folder (start, heuristic cap) | runs | compared with | result |
|---|---|---|---|
| `post-fold-check/` (artificial columns, no cap) | 20 layer configs + `example-mip` | `small/` for `layer-{cpp,python}-*-{exact,heuristic}` | equal (12 of 12) |
| `post-fold-check-cap100/` (artificial columns, cap 100) | 8 heuristic configs | `heuristic-cap100/` for `cpp`, `python` | equal (6 of 6) |
| `post-fold-check-single/` (`COLGEN_INIT=single`) | exact × 3 loops × highspy, gurobipy; tabu × cpp, purepython | `single-small/` (cpp, python), `full-small-single/` (cpp, purepython, tabu) | equal (8 of 8) |
| `post-fold-check-bigm/` (`COLGEN_INIT=bigm`) | the same 8 | `full-small-bigm/` (cpp, purepython, tabu) | equal (6 of 6) |

No recorded run to compare with (equal LP everywhere, 191.8136):

- `layer-purepython-*` with the default start and `layer-purepython-*-heuristic`: never run before. The pure layer's
  automatic cost is 1e4 × the cost scale, the explicit cost of `bigm`: its default-start exact runs give the `bigm`
  numbers (16 / 599 with highspy, 23 / 691 with gurobipy).
- `*-tabu` with the default start, and `layer-python-*` with `bigm`: not recorded.

`example-mip` differs from `small/` (34 iterations, 783 columns against 35, 777), LP equal. It is not deterministic
here: three reruns gave 34 / 783, 32 / 843 and 23 / 661. It runs `examples/python/vrp`, unchanged by the fold.

Times are single runs on a machine that was not idle.
