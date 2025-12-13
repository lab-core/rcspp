// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/composition/composition.hpp"

namespace rcspp {

// Specialization for ResourceComposition
template <typename... ResourceTypes>
    requires(std::derived_from<ResourceTypes, ResourceBase<ResourceTypes>> && ...)
class Extender<ResourceBaseComposition<ResourceTypes...>>
    : public ExtenderPrototype<Extender<ResourceBaseComposition<ResourceTypes...>>,
                               ResourceBaseComposition<ResourceTypes...>>,
      public Composition<Extender, ResourceTypes...> {
        using Prototype = ExtenderPrototype<Extender, ResourceBaseComposition<ResourceTypes...>>;

    public:
        Extender() = default;

        Extender(const ResourceBaseComposition<ResourceTypes...>& resource_base,
                 std::unique_ptr<ExtensionFunction<ResourceBaseComposition<ResourceTypes...>>>
                     extension_function,
                 const size_t arc_id)
            : Prototype(resource_base, std::move(extension_function), arc_id) {}

        Extender(std::unique_ptr<ExtensionFunction<ResourceBaseComposition<ResourceTypes...>>>
                     extension_function,
                 const size_t arc_id)
            : Prototype(std::move(extension_function), arc_id) {}

        [[nodiscard]] auto clone(const Arc<ResourceBaseComposition<ResourceTypes...>>& arc) const
            -> auto {
            auto new_extender = Prototype::clone(arc);
            this->apply(*new_extender, [&arc](const auto& extenders, auto& new_extenders) {
                for (const auto& extender : extenders) {
                    new_extenders.emplace_back(extender->clone(arc));
                }
            });

            return new_extender;
        }
};
}  // namespace rcspp
