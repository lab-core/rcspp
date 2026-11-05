// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <vector>

namespace rcspp {

/// @brief The components a dominance test leaves out: relaxed dominance compares labels over the
///        others only, so more labels are dropped.
///
/// Components are numbered as the model checks number them: type slots in the order of the
/// `ResourceGraph`'s template parameters, then registration order within a type. A component the
/// mask was not sized for is compared. An empty mask leaves nothing out: dominance is exact.
class DominanceMask {
    public:
        DominanceMask() = default;

        /// @brief Every one of @p num_components components compared.
        explicit DominanceMask(size_t num_components) : ignored_(num_components, false) {}

        /// @brief Leaves component @p component out.
        ///
        /// @throws std::out_of_range when the mask was not sized for @p component.
        void ignore(size_t component) {
            if (!ignored_.at(component)) {
                ignored_[component] = true;
                ++count_;
            }
        }

        /// @brief Whether component @p component is left out.
        [[nodiscard]] bool ignores(size_t component) const {
            return component < ignored_.size() && ignored_[component];
        }

        /// @brief Whether nothing is left out.
        [[nodiscard]] bool empty() const { return count_ == 0; }

        /// @brief How many components are left out.
        [[nodiscard]] size_t count() const { return count_; }

    private:
        std::vector<bool> ignored_;
        size_t count_ = 0;
};

}  // namespace rcspp
