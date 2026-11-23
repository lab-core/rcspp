# feat(logger): no ANSI colors when the output is not a terminal

<!-- markdownlint-disable MD013 -->

## Summary

`Logger` always writes ANSI color codes around each console message (`cpp/rcspp/utils/logger.hpp:128`, `color_for` at line 240 and `color_reset` at line 258). When the output is redirected (to a file, `tee`, or a CI log), it is full of escape sequences such as `\033[32m`, and every consumer has to strip them first. The file sink (`file_stream_`, line 132) already writes no colors.

## Proposed change

- Write colors only when stdout is a terminal (`isatty(fileno(stdout))` on POSIX, `_isatty(_fileno(stdout))` on Windows).
- Let users override the detection: honor the `NO_COLOR` environment variable ([no-color.org](https://no-color.org)), and add a `color` option to `Logger::init` (auto / always / never).

## Done when

A program that logs through `Logger`, with its output redirected to a file, writes no escape sequences, while an interactive terminal still shows colors.

Checked on `main` @ `6367ee56`.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
