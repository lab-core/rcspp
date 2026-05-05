// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp {

// Just a placeholder to store the template types
template <typename... ResourceTypes>
class ResourceTypeComposition {
    public:
        ResourceTypeComposition() = default;

        void reset() {}

        [[nodiscard]] ResourceTypeComposition get_value() const { return *this; }
};

template <typename T>
inline constexpr bool is_resource_base_composition_v = false;
template <typename... ResourceTypes>
inline constexpr bool is_resource_base_composition_v<ResourceTypeComposition<ResourceTypes...>> =
    true;

}  // namespace rcspp
