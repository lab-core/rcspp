# fix(ci): the benchmark workflow can't build without Gurobi on ubuntu-latest

## Summary

`.github/workflows/benchmark.yml` runs on `ubuntu-latest` and builds `--target rcspp-vrp-benchmark` (and `rcspp-vrp-benchmark-large`). Its own comment says "Gurobi must be installed on the runner and GUROBI_HOME must be set", but no step installs Gurobi. Without it, `examples/cpp/vrp/CMakeLists.txt:29-31` prints "Gurobi not found; skipping VRP targets" and returns, so the target doesn't exist and the build step fails. The workflow has never been run (`gh run list --workflow benchmark.yml` is empty), so this is inferred from the files, not observed.

## Options

| option | effect |
|---|---|
| self-hosted runner with Gurobi | works as written; needs a machine and a license |
| Gurobi installed in the job + a license secret (for example a web license service key) | stays on GitHub runners; needs a license that allows CI use |
| an open-source master solver for the benchmark (HiGHS, once the column generation layer exists) | no license; results comparable only between runs on the same solver |

## Done when

A manual run of the workflow builds and runs the small benchmark, or the workflow states clearly that it needs a self-hosted runner.

Checked on `main` @ `129b15e0`.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
