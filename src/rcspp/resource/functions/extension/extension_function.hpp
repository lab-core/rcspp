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
class Extender;

template <typename ResourceType>
class ExpansionFunction {
    public:
        virtual ~ExpansionFunction() = default;

        virtual void extend(const Resource<ResourceType>& resource,
                            const Extender<ResourceType>& extender,
                            Resource<ResourceType>* extended_resource) = 0;

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<ExpansionFunction> = 0;

        virtual auto create(size_t arc_id) -> std::unique_ptr<ExpansionFunction> {
            auto new_extension_function = clone();

            new_extension_function->arc_id_ = arc_id;

            new_extension_function->preprocess();

            return new_extension_function;
        }

        size_t arc_id_;

    protected:
        virtual void preprocess() {}
};
}  // namespace rcspp
