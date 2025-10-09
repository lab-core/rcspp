// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/base/resource_base.hpp"

template <typename ResourceType>
// requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Arc;

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Node {
    public:
        explicit Node(size_t node_id) : id(node_id) {}

        const size_t id;

        std::vector<Arc<ResourceType>*> in_arcs;
        std::vector<Arc<ResourceType>*> out_arcs;

        std::unique_ptr<Resource<ResourceType>> resource;
};
