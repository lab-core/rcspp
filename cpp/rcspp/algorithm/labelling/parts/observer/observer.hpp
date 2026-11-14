// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/labelling/parts/extension_outcome.hpp"
#include "rcspp/graph/arc.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief What a search tells of every extension it tries, the unreachable ones included.
///
/// @tparam ObserverType The observer.
/// @tparam ResourceType The resource type carried by labels.
template <typename ObserverType, typename ResourceType>
concept Observer = requires(ObserverType observer, const Label<ResourceType>& label,
                            const Arc<ResourceType>& arc, ExtensionOutcome outcome) {
    observer.on_extension(label, arc, outcome);
};  // NOLINT(whitespace/newline)

}  // namespace rcspp::parts
