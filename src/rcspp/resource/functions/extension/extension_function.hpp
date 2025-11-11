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
class Arc;

template <typename ResourceType>
class ExpansionFunction {
    public:
        virtual ~ExpansionFunction() = default;

        virtual void extend(const Resource<ResourceType>& resource,
                            const Extender<ResourceType>& extender,
                            Resource<ResourceType>* extended_resource) = 0;

        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<ExpansionFunction> = 0;

        template <typename GraphResourceType>
        auto create(const Arc<GraphResourceType>& arc) -> std::unique_ptr<ExpansionFunction> {
            auto new_extension_function = clone();

            new_extension_function->arc_id_ = arc.id;

            new_extension_function->preprocess(arc);

            return new_extension_function;
        }

        size_t arc_id_;

    protected:
        template <typename GraphResourceType>
        void preprocess(const Arc<GraphResourceType>& arc) {}
};
}  // namespace rcspp
