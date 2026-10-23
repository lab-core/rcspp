# refactor(graph)!: make Row::coefficient a double and reject non-finite coefficients

## Summary

`Row::coefficient` is `long double` (`cpp/rcspp/graph/row.hpp:20`). Everything around it is `double`: arc costs, duals, the LP solvers that consume the columns, and even the column pool's own storage. The extra precision is thrown away at every boundary, while `long double` makes results platform-dependent, doubles the size of `Row` on Linux and macOS x86, and is very slow on Linux ARM. It has been `long double` since the first commit, with no recorded reason.

## Where it is used

| place | what happens |
|---|---|
| `cpp/rcspp/graph/row.hpp:20` | `long double coefficient;` |
| `cpp/rcspp/algorithm/algorithm.hpp:725` | column build: `std::unordered_map<size_t, long double> row_map` |
| `cpp/rcspp/resource/resource_graph.hpp` ~527 | `double reduced_cost -= row.coefficient * duals[i]`: each step is computed in `long double`, then narrowed back to `double` |
| `cpp/rcspp/algorithm/solution_pool.hpp:1016-1024` | pool pricing widens the **stored `double`** coefficients back to `long double` "to match Row::coefficient's original precision"; the pool already stores `double` (`lp_.row_coefs`), so that precision is already gone |
| `python/bindings/graph.cpp:192`, `python/bindings/graph_impl.hpp:418` | Python `float` / numpy `float64` widened to `long double` |
| `tests/cpp/test_solution_pool.hpp:207`, `tests/cpp/vrp_subproblem/vrp_subproblem.cpp:158`, `0.1L` literals | tests of the `long double` path |

## Why change it

- **Results depend on the platform.** `long double` means different things depending on the compiler and machine:
  - 64-bit (identical to `double`) with MSVC/clang-cl on Windows and on Apple ARM;
  - 80-bit x87 extended precision stored in 16 bytes with GCC/Clang on x86-64 Linux and Intel macOS;
  - 128-bit IEEE quad, emulated in software, with GCC on Linux aarch64.

  The same instance can give last-bit differences between Linux and Windows, which is noise in benchmarks and in golden tests.
- **Memory.** `sizeof(Row)` is 16 bytes on Windows but 32 on x86-64 Linux (8 for `size_t`, 8 of padding, 16 for the `long double`). Every arc stores a `std::vector<Row>`, and so does every column.
- **Speed.** x87 arithmetic doesn't vectorize, and quad precision on aarch64 is emulated. Hot spots: `update_reduced_costs` (every row of every arc, every column generation iteration), column building, pool pricing. Not measured yet: the PR should measure it.
- **No precision gain.** Inputs are `double` (the C++ API and Python `float`), duals are `double`, and consumers are `double`. Coefficients are typically small integers (visit counts), which are exact in `double`.

## Proposed change

1. `double coefficient;` and the same type at every site above; accumulate in `double` in the column build and in pool pricing (drop the widening comment).
2. `static_assert(sizeof(Row) == 16)` next to the struct.
3. **Reject non-finite coefficients** (NaN, ±inf) where rows enter the graph: `add_arc(..., rows)`, `add_rows_to_arc`, `add_rows` (numpy). Throw `std::invalid_argument` naming the arc and row, or add the check to the pre-solve model checks (#46). Overflow from the type change itself is not a risk: no coefficient comes near `double`'s range (~1.8e308).
4. Update the tests: `0.1L` → `0.1`; keep L-7 (`PriceFractionalCoefficients`) as a fractional-coefficient test with a new rationale; add a test for the rejection.

## Compatibility

Breaking in principle (the field type changes), hence the `!`. In practice:

- `Row{i, 1.0}` keeps compiling;
- `Row{i, some_long_double_variable}` becomes a narrowing error in braced initialization;
- Python is unchanged (`float` either way).

## Done when

- All tests pass.
- The VRP benchmark gives the same LP values to 1e-9 on Linux and Windows (on Windows they should be bit-identical, since `long double` is already `double` there).
- `sizeof(Row) == 16` holds on every platform.
- A NaN or infinite coefficient is rejected with a clear message.

Timing: after the open RouteOpt stack (#41–#49) merges, since #41 touches `algorithm.hpp`.

Checked on `main` @ `129b15e0`.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
