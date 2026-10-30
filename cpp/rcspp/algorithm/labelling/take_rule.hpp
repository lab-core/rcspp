// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::detail {

/// @brief What the kernel does with a label it has taken from a schedule.
enum class Verdict : std::uint8_t {
    Extend,           ///< extend it along every arc its direction walks
    Keep,             ///< leave it stored: an end-node, boundary or abandoned label
    Release,          ///< release it through its reference counts
    EraseAndRelease,  ///< erase it from its node, then release it
};

/// @brief The take rule's answer for one label.
struct Decision {
        Verdict verdict = Verdict::Extend;
        bool record = false;  ///< an end-node label to record now
};

/// @brief The checks a label goes through when it is taken, written once for every search.
///
/// Two halves, run in this order: @ref live, whose answers can change while the label waits
/// (dominated since it was queued; the incumbent), then @ref settled, whose answers never change
/// for it (at an end node; infinite cost; past the half-way point). Every schedule applies the
/// per-node quota itself.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam Direction    The direction the label was extended in.
template <typename ResourceType, typename Direction>
struct TakeRule {
        /// @brief The checks whose answer can change while the label waits.
        ///
        /// A dominated label is released. With @c prune_by_incumbent, a label whose cost is at or
        /// above the incumbent is erased and released; on a bidirectional side the cost includes
        /// the side's completion bound, since with reduced costs a half's own cost is not a lower
        /// bound on a full path (a single-direction search prunes on its cost alone, A* included:
        /// its bound orders, it does not prune).
        ///
        /// @return Extend when the label passes; otherwise what to do with it.
        template <typename SearchType>
        static Decision live(const Label<ResourceType>& label, const SearchType& /*search*/,
                             const SearchContext<ResourceType>& ctx) {
            if (label.dominated) {
                return {.verdict = Verdict::Release};
            }
            if (ctx.params->prune_by_incumbent &&
                label.get_cost() + (ctx.abandons() ? (*ctx.bound)(label) : 0.0) >= *ctx.incumbent) {
                return {.verdict = Verdict::EraseAndRelease};
            }
            return {};
        }

        /// @brief The checks whose answer never changes for the label.
        ///
        /// An end-node label lowers the incumbent when it is below the upper bound, and is
        /// recorded now when the search's retention collects at take or the params return
        /// dominated solutions; the caller records it and then asks
        /// @ref SearchContext::budget_spent whether to stop. It stays stored.
        ///
        /// @return Extend when the label passes; otherwise what to do with it.
        template <typename SearchType>
        static Decision settled(const Label<ResourceType>& label, const SearchType& /*search*/,
                                const SearchContext<ResourceType>& ctx) {
            if (Direction::is_end_node(label.get_end_node())) {
                if (label.get_cost() < ctx.upper_bound) {
                    *ctx.incumbent = std::min(*ctx.incumbent, label.get_cost());
                }
                return {.verdict = Verdict::Keep,
                        .record = SearchType::retention_type::collects_at_take ||
                                  ctx.params->return_dominated_solutions};
            }
            if (std::isinf(label.get_cost())) {
                return {.verdict = Verdict::EraseAndRelease};
            }
            if (ctx.half_way != nullptr && ctx.half_way->template past<Direction>(label)) {
                return {.verdict = Verdict::Keep};
            }
            return {};
        }
};

}  // namespace rcspp::detail
