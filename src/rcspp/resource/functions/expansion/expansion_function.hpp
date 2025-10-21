// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <iostream>
#include <memory>
#include <utility>

namespace rcspp {

template <typename ResourceType>
class Resource;

template <typename ResourceType>
class Expander;

template <typename ResourceType>
class ExpansionFunction {
    public:
        virtual ~ExpansionFunction() = default;

        virtual void expand(const Resource<ResourceType>& resource,
                            const Expander<ResourceType>& expander,
                            Resource<ResourceType>& expanded_resource) = 0;

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<ExpansionFunction> = 0;

        virtual auto create(size_t arc_id) -> std::unique_ptr<ExpansionFunction> {
            auto new_expansion_function = clone();

            new_expansion_function->arc_id_ = arc_id;

            new_expansion_function->preprocess();

            return new_expansion_function;
        }

        size_t arc_id_;

    protected:
        virtual void preprocess() {}
};
}  // namespace rcspp