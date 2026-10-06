// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rcspp {

/// @brief What the model checks found: one line per problem, empty when the model passes.
struct ModelReport {
        /// @brief One line per problem, naming the component or the arc and the fix.
        std::vector<std::string> problems;

        /// @brief Whether no check found a problem.
        [[nodiscard]] bool ok() const { return problems.empty(); }
};

/// @brief Thrown when a model fails the checks its search needs.
///
/// Still a @c std::runtime_error, so Python sees a @c RuntimeError. @c what() is the header
/// followed by one indented line per problem; @ref problems lists them without parsing.
class ModelRefused : public std::runtime_error {
    public:
        /// @brief Builds the message from @p header and @p problems.
        ///
        /// @param header   The first line, naming the search, e.g.
        ///                 "a bidirectional search cannot run on this model:".
        /// @param problems One line per problem.
        ModelRefused(const std::string& header, std::vector<std::string> problems)
            : std::runtime_error(message(header, problems)), problems_(std::move(problems)) {}

        /// @brief The problems, one line each.
        [[nodiscard]] const std::vector<std::string>& problems() const { return problems_; }

    private:
        [[nodiscard]] static std::string message(const std::string& header,
                                                 const std::vector<std::string>& problems) {
            std::string text = header;
            for (const auto& problem : problems) {
                text += "\n  - " + problem;
            }
            return text;
        }

        std::vector<std::string> problems_;
};

}  // namespace rcspp
