// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"

namespace rcspp::parts {

/// @brief Keeps every child: a plain dive.
struct KeepAllChildren {
        template <typename ResourceType>
        void select(std::vector<StoredLabel<ResourceType>>& /*children*/,
                    std::vector<StoredLabel<ResourceType>>& /*rejected*/,
                    const detail::SearchContext<ResourceType>& /*context*/) const {}
};

}  // namespace rcspp::parts
