// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

namespace rcspp {

template <typename ResourceType>
class Resource;

template <typename ResourceType>
class CostFunction {
    public:
        virtual ~CostFunction() = default;

        [[nodiscard]] virtual auto get_cost(const Resource<ResourceType>& resource) const
            -> double = 0;

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<CostFunction> = 0;

        auto create(const size_t node_id) -> std::unique_ptr<CostFunction> {
            auto new_cost_function = clone();
            new_cost_function->preprocess(node_id);
            return new_cost_function;
        }

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};
}  // namespace rcspp
