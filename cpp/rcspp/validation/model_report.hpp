// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/algorithm/search_direction.hpp"

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
/// Derives from @c std::runtime_error, so Python sees a @c RuntimeError. @c what() is a line
/// naming the search, e.g. "a bidirectional search cannot run on this model:", followed by one
/// indented line per problem; @ref problems lists them without parsing.
class ModelRefused : public std::runtime_error {
    public:
        /// @brief Builds the message for a search in @p direction from @p problems.
        ///
        /// @param direction The direction of the search that refuses the model.
        /// @param problems  One line per problem.
        ModelRefused(SearchDirection direction, std::vector<std::string> problems)
            : std::runtime_error(message(direction, problems)), problems_(std::move(problems)) {}

        /// @brief The problems, one line each.
        [[nodiscard]] const std::vector<std::string>& problems() const { return problems_; }

    private:
        [[nodiscard]] static std::string message(SearchDirection direction,
                                                 const std::vector<std::string>& problems) {
            std::string text =
                "a " + std::string(to_string(direction)) + " search cannot run on this model:";
            for (const auto& problem : problems) {
                text += "\n  - " + problem;
            }
            return text;
        }

        std::vector<std::string> problems_;
};

}  // namespace rcspp
