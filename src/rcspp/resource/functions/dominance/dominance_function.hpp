// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

template <typename ResourceType>
class Resource;

template <typename ResourceType>
class DominanceFunction {
    public:
        virtual ~DominanceFunction() = default;

        virtual auto check_dominance(const Resource<ResourceType>& lhs_resource,
                                     const Resource<ResourceType>& rhs_resource) -> bool = 0;

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<DominanceFunction> = 0;

        auto create(const size_t node_id) -> std::unique_ptr<DominanceFunction> {
            auto new_dominance_function = clone();

            new_dominance_function->node_id_ = node_id;

            new_dominance_function->preprocess();

            return new_dominance_function;
        }

        virtual void reset(const size_t node_id) {
            node_id_ = node_id;

            preprocess();
        }

    protected:
        size_t node_id_;

        virtual void preprocess() {}
};
