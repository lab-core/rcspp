// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/preprocessor.hpp"

namespace rcspp {

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class ShortestPathPreprocessor : public Preprocessor<ResourceType> {
    public:
        ShortestPathPreprocessor(Graph<ResourceType>* graph, double upper_bound)
            : Preprocessor<ResourceType>(graph), upper_bound_(upper_bound) {}

    protected:
        double upper_bound_;

        bool remove_arc(const Arc<ResourceType>& arc) override { return false; }
};
}  // namespace rcspp
