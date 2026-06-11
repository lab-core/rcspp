// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <optional>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Extension function that extends a numerical resource by addition.
///
/// The extended value is computed as `resource + extender_value`.  An optional
/// lower bound @p min_value can be provided; if set, the result is clamped to
/// `max(min_value, sum)`.  Typical use: accumulating arc costs, travel times,
/// or distances along a path in the RCSPP labelling algorithm.
///
/// @tparam ResourceType A NumericalResource-compatible type whose `get_value()`
///                      returns an arithmetic value and which supports `set_value()`.
template <typename ResourceType>
class AdditionExtensionFunction
    : public Clonable<AdditionExtensionFunction<ResourceType>, ExtensionFunction<ResourceType>> {
        using ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>;

    public:
        /// @brief Constructs an AdditionExtensionFunction with an optional minimum value clamp.
        ///
        /// @param min_value If provided, the extended value will never be less than this floor.
        explicit AdditionExtensionFunction(std::optional<ValueType> min_value = std::nullopt)
            : min_value_(min_value) {}

        /// @brief Extends @p resource by adding @p extender_value, storing the result in
        /// @p extended_resource.
        ///
        /// @param resource           The current resource state of the label being extended.
        /// @param extender_value     The arc's resource consumption to add.
        /// @param extended_resource  Output: receives the new resource value.
        void extend(const ResourceType& resource, const ResourceType& extender_value,
                    ResourceType* extended_resource) override {
            auto sum_value = resource.get_value() + extender_value.get_value();
            if (min_value_.has_value()) {
                sum_value = std::max(min_value_.value(), sum_value);
            }
            extended_resource->set_value(sum_value);
        }

    private:
        std::optional<ValueType> min_value_;
};
}  // namespace rcspp
