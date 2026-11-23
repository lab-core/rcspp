# perf(python): the search called from Python runs 5-6 % slower than from C++ with tabu

<!-- markdownlint-disable MD013 -->

## Summary

The same C++ search takes 5 to 6 % longer when it is called through the Python bindings than when a C++ program calls it, with tabu pricing. Measured with the column generation benchmark on Linux (Solomon VRPTW, 50 customers, a tabu stage then an exact one): R201_50 prices in 82.6 ms an iteration from a C++ program (`cpp-native` layer) against 88.1 ms from Python (`cpp-bindings` layer). Both run the same C++ column generation example, the same search on the same graph, with the same iterations and columns; only the caller differs.

## Suspects, not measured

- The interrupt checks the bindings add for Ctrl-C (`ActiveCall`, the `should_stop` callback): how often the search polls them, and what each poll costs.
- The extension module's build: optimization flags, link-time optimization, or other settings that differ from the C++ program's build.
- Releasing and re-acquiring the GIL around each call, which matters only if the calls are short.

## To do

Find which of these accounts for the gap (for instance, the same search from Python with the interrupt check disabled, and the C++ program built with the extension's flags); then fix it or document it.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
