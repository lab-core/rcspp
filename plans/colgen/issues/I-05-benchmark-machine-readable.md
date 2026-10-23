# feat(benchmark): machine-readable results (CSV or JSON lines)

## Summary

`rcspp-vrp-benchmark` and `rcspp-vrp-benchmark-large` only print a log and a formatted table (`format_benchmark_table` in `examples/cpp/benchmark_common.hpp`). Comparing runs, plotting, or tracking regressions means parsing that table out of a colored log.

## Proposed change

Add an option (for example `--output results.jsonl`) that writes one record per (instance, pricing algorithm):

| field | meaning |
|---|---|
| `instance`, `algorithm` | what ran |
| `lp_value`, `proven` | final LP value; whether the last pricing round proved it optimal |
| `iterations`, `columns` | column generation iterations; columns generated |
| `lp_time_s`, `pricing_time_s`, `total_time_s` | time split |
| `relaxation`, `ng_size`, `distance_rounding` | model settings that change the LP value |
| `commit`, `build_type`, `timestamp` | provenance |

JSON lines (one object per line) appends cleanly across runs and reads with pandas or `jq`; CSV is an alternative if preferred.

## Done when

A benchmark run writes the file, and two runs can be compared without parsing the log.

Checked on `main` @ `129b15e0`.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
