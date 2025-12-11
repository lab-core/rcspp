// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/composition/composition.hpp"

namespace rcspp {

// Specialization for ResourceComposition
// TODO(patrick): Find a way to avoid code duplication
template <typename... ResourceTypes>
// requires (std::derived_from<ResourceTypes, ResourceBase<ResourceTypes>> && ...)
class ExtenderComposition : public Extender<ResourceBaseComposition<ResourceTypes...>>,
                            public Composition<Extender<ResourceTypes>...> {
        friend class ResourceCompositionFactory<ResourceTypes...>;

    public:
        ExtenderComposition() = default;

        ExtenderComposition(
            const ResourceBaseComposition<ResourceTypes...>& resource_base,
            std::unique_ptr<ExtensionFunction<ResourceBaseComposition<ResourceTypes...>>>
                extension_function,
            const size_t arc_id)
            : Extender<ResourceBaseComposition<ResourceTypes...>>(
                  resource_base, std::move(extension_function), arc_id) {}

        ExtenderComposition(
            std::unique_ptr<ExtensionFunction<ResourceBaseComposition<ResourceTypes...>>>
                extension_function,
            const size_t arc_id)
            : Extender<ResourceBaseComposition<ResourceTypes...>>(std::move(extension_function),
                                                                  arc_id) {}

        [[nodiscard]] auto clone(const Arc<ResourceBaseComposition<ResourceTypes...>>& arc) const
            -> auto {
            auto new_extender = std::make_unique<ExtenderComposition<ResourceTypes...>>(
                *this,
                this->extension_function_->create(arc),
                arc.id);

            new_extender->apply(new_extender->components_,
                                [&arc](const auto& extenders, auto& new_extenders) {
                                    for (const auto& extender : extenders) {
                                        new_extenders.emplace_back(extender.clone(arc));
                                    }
                                });

            return new_extender;
        }
};
}  // namespace rcspp
