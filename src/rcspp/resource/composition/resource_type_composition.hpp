// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <stdexcept>
#include <string>

#include "rcspp/resource/base/resource_type.hpp"

namespace rcspp {

// Just a placeholder to store the template types
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class ResourceTypeComposition {
    public:
        ResourceTypeComposition() = default;

        void reset() {}

        [[nodiscard]] const ResourceTypeComposition& get_value() const { return *this; }

        template <typename... Args>
        void set_value(Args&&... /* args */) {
            throw std::logic_error("ResourceTypeComposition::set_value(...) is not available");
        }

        [[nodiscard]] std::string to_string() const { return ""; }
};

template <typename T>
inline constexpr bool is_resource_base_composition_v = false;
template <typename... ResourceTypes>
inline constexpr bool is_resource_base_composition_v<ResourceTypeComposition<ResourceTypes...>> =
    true;

template <typename T>
concept ResourceCompositionTypeConcept = is_resource_base_composition_v<T>;

}  // namespace rcspp
