# Comment on #105: the color reset is written after the line break

<!-- markdownlint-disable MD013 -->

A comment to add to #105 (no ANSI colors when the output is not a terminal), not a new issue.

## Comment

One more effect of the colors, which detecting a terminal would not fix on its own: `Logger::log` writes the color code, the message with its line break, then the reset code (`cpp/rcspp/utils/logger.hpp:128`). The reset therefore starts the *next* line of the output. A program that logs a warning and then prints a line of its own gives that line a leading `\033[0m`, so a reader that matches lines by their start (`line.startswith("RESULT ")`) misses it.

We hit it in the column generation benchmark: every `cpp-native` run whose search logged a warning (for example a bidirectional search without a usable half-way point) lost its result. The runner now strips the codes before reading.

Writing the reset before the line break (`color + header + payload-without-newline + reset + "\n"`) keeps each line's codes on that line, whatever the terminal detection decides.
