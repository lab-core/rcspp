// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"
#include "rcspp/resource/functions/extension/threshold_form.hpp"

namespace rcspp {

/// @brief A capacity: a sum along the path that must stay under a per-node cap (a load, a duration,
///        any limited consumption).
///
/// Forward it adds like @c AdditionExtensionFunction. Backward it is a threshold: a backward label
/// holds how much the prefix may still have consumed at the node, `min(cap_u, b(v) - q)`, and
/// starts at the sink's cap. So the join only compares `forward <= backward`, and a capacity can be
/// the half-way clock, which an addition cannot.
///
/// @note The per-node bounds here are a fallback. When paired through
///       @c ResourceGraph::add_resource, the backward clamp reads caps from the feasibility
///       function; this map is used only at nodes where that function states none.
///
/// @tparam ResourceType A NumericalResource-compatible type whose value type is arithmetic.
/// @tparam ValueType    Deduced value type of the resource (default: `ResourceType::get_value()`
///                      return type after decay).
template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class CapacityExtensionFunction
    : public Clonable<
          CapacityExtensionFunction<ResourceType, ValueType>,
          TranslationThresholdForm<ResourceType, ExtensionFunction<ResourceType>, ValueType>,
          ExtensionFunction<ResourceType>> {
        // Backward extension subtracts, so an unsigned type would wrap around.
        static_assert(std::is_signed_v<ValueType>,
                      "CapacityExtensionFunction requires a signed value type");

    public:
        /// @brief A capacity with per-node caps (@p max_by_node_id) and a default.
        ///
        /// The paired feasibility function's caps take precedence where it states one.
        ///
        /// @param max_by_node_id Per-node upper bounds, used where the paired feasibility
        ///                       function states none (see the class note). May be empty.
        /// @param default_max    Bound used at nodes absent from the map. Defaults to half of the
        ///                       value type's maximum so the forward addition cannot overflow.
        explicit CapacityExtensionFunction(
            std::map<size_t, ValueType> max_by_node_id = {},
            ValueType default_max = std::numeric_limits<ValueType>::max() / 2)
            : max_by_node_id_(max_by_node_id.empty()
                                  ? nullptr
                                  : std::make_shared<const std::map<size_t, ValueType>>(
                                        std::move(max_by_node_id))),
              default_max_(default_max) {}

    protected:
        /// @brief None: a capacity does not wait.
        ///
        /// @param node_id Index of the node the forward extension arrives at (unused).
        /// @return @c std::nullopt.
        [[nodiscard]] std::optional<ValueType> lower_bound_at(size_t /*node_id*/) const final {
            return std::nullopt;
        }

        /// @brief This node's own cap: backward labels are clamped to it, and start at it at a
        ///        sink, where the paired feasibility function states none.
        ///
        /// @param node_id Index of the node the backward extension arrives at.
        /// @return The node's upper bound, or @c default_max_ if it has none.
        [[nodiscard]] std::optional<ValueType> upper_bound_at(size_t node_id) const final {
            if (max_by_node_id_ == nullptr) {
                return default_max_;
            }
            auto it = max_by_node_id_->find(node_id);
            return it != max_by_node_id_->end() ? it->second : default_max_;
        }

    private:
        std::shared_ptr<const std::map<size_t, ValueType>> max_by_node_id_;
        ValueType default_max_;
};
}  // namespace rcspp
