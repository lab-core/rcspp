// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/labelling/parts/extension_outcome.hpp"
#include "rcspp/graph/arc.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief The default observer: no event is recorded, and the compiler removes the calls. A test
///        observer records them.
struct NoObserver {
        template <typename ResourceType>
        void on_extension(const Label<ResourceType>& /*label*/, const Arc<ResourceType>& /*arc*/,
                          ExtensionOutcome /*outcome*/) {}
};

}  // namespace rcspp::parts
