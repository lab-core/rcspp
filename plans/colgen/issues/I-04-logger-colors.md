# feat(logger): no ANSI colors when the output is not a terminal

## Summary

`Logger` always writes ANSI color codes around each message (`cpp/rcspp/utils/logger.hpp:128`, `color_for` / `color_reset`). When the output is redirected (to a file, `tee`, or a CI log), it is full of escape sequences such as `\e[32m`, and every consumer has to strip them first.

## Proposed change

- Write colors only when stdout is a terminal (`isatty(fileno(stdout))` on POSIX, `_isatty(_fileno(stdout))` on Windows).
- Let users override the detection: honor the `NO_COLOR` environment variable ([no-color.org](https://no-color.org)), and add a `color` option to `Logger::init` (auto / always / never).
- The file sink (`file_stream_`) never gets colors.

## Done when

`rcspp-vrp-benchmark 1 > out.log` produces a log without escape sequences, while an interactive terminal still shows colors.

Checked on `main` @ `129b15e0`.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
