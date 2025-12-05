// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

namespace rcspp {

template <typename ResourceType>
class Resource;

template <typename ResourceType>
class FeasibilityFunction {
    public:
        virtual ~FeasibilityFunction() = default;

        [[nodiscard]] virtual auto is_feasible(const Resource<ResourceType>& resource) -> bool = 0;

        [[nodiscard]] virtual auto is_back_feasible(const Resource<ResourceType>& resource)
            -> bool {
            return is_feasible(resource);
        }

        [[nodiscard]] virtual auto can_be_merged(const Resource<ResourceType>& resource,
                                                 const Resource<ResourceType>& back_resource)
            -> bool {
            throw std::runtime_error("FeasibilityFunction::merge not implemented");
        };

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<FeasibilityFunction> = 0;

        virtual auto create(const size_t node_id) -> std::unique_ptr<FeasibilityFunction> {
            auto new_feasibility_function = clone();
            new_feasibility_function->preprocess(node_id);
            return new_feasibility_function;
        }

        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        virtual void preprocess(size_t node_id) {}
};
}  // namespace rcspp
