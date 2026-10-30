// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <list>
#include <queue>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/policies.hpp"
#include "rcspp/graph/arc.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::parts {

/// @brief An erase callable for a schedule told to release what it stored aside, when the caller
///        has no container to erase from: it erases nothing.
template <typename ResourceType>
struct IgnoreErase {
        void operator()(const LabelIterator<ResourceType>& /*position*/) const {}
};

/// @brief What every schedule provides: the order labels are taken in, the per-node quota, and
///        how it trims itself.
///
/// @c initialize starts a solve; @c push hands it a newly kept label; @c size counts the labels
/// waiting; @c prepare_next_phase gives back what a cut stored aside; @c trim keeps about
/// @c limit labels per node, under memory pressure; @c release_stored_aside releases what a cut
/// stored aside, erasing each through the callable; @c clear forgets every label (the pool owns
/// them).
///
/// A schedule loses no label silently: every label pushed is handed out, released as dominated,
/// or cut through @ref CutLabels, so that the solve's status sees the cut. A schedule releases
/// through the pool and erases only through the callable it is handed.
///
/// @tparam ScheduleType The schedule.
/// @tparam ResourceType The resource type carried by labels.
template <typename ScheduleType, typename ResourceType>
concept Schedule = requires(ScheduleType schedule, const ScheduleType const_schedule,
                            detail::SearchContext<ResourceType>& ctx,
                            const StoredLabel<ResourceType>& stored, size_t limit) {
    schedule.initialize(ctx);
    schedule.push(stored);
    { const_schedule.size() } -> std::convertible_to<size_t>;
    schedule.prepare_next_phase(ctx);
    schedule.trim(ctx, limit);
    schedule.release_stored_aside(ctx, IgnoreErase<ResourceType>{});
    schedule.clear();
};

/// @brief A schedule that hands out one label at a time: @c next returns it with its position in
///        its node's container, or a null label once none is left.
template <typename ScheduleType, typename ResourceType>
concept LabelSchedule = Schedule<ScheduleType, ResourceType> &&
                        requires(ScheduleType schedule, detail::SearchContext<ResourceType>& ctx) {
                            { schedule.next(ctx) } -> std::same_as<StoredLabel<ResourceType>>;
                        };

/// @brief Whether @p ScheduleType's order reads the search's completion bound: a schedule says so
///        with `static constexpr bool reads_bound = true`; one that does not say reads none.
template <typename ScheduleType>
inline constexpr bool reads_bound_v = [] {
    if constexpr (requires { ScheduleType::reads_bound; }) {
        return ScheduleType::reads_bound;
    } else {
        return false;
    }
}();

// ─── The helpers the schedules share ────────────────────────────────────────────────────────

/// @brief The labels taken at each node, against the per-node quota.
class PerNodeQuota {
    public:
        /// @brief No label taken yet, at any of @p num_nodes nodes.
        void reset(size_t num_nodes) { taken_.assign(num_nodes, 0); }

        /// @brief No label taken yet, for the next phase.
        void restart() { std::ranges::fill(taken_, 0); }

        /// @brief Whether one more label may be taken at node position @p pos, counting it if
        ///        so.
        [[nodiscard]] bool may_take(size_t pos, size_t quota) {
            size_t& taken = taken_.at(pos);
            if (taken < quota) {
                ++taken;
                return true;
            }
            return false;
        }

    private:
        std::vector<size_t> taken_;
};

/// @brief What a cut (the per-node quota, memory pressure) takes out of a schedule: stored aside
///        for the next phase by a single-direction search, abandoned (left in its container,
///        never extended) and counted by a bidirectional side.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class CutLabels {
    public:
        /// @brief Takes a non-dominated label out of the work, as @p ctx's role says.
        void cut(detail::SearchContext<ResourceType>& ctx,
                 const StoredLabel<ResourceType>& stored) {
            if (!ctx.abandons()) {
                aside_.push_back(stored);
            } else if (ctx.abandoned != nullptr) {
                ++*ctx.abandoned;
            }
        }

        /// @brief Hands every label stored aside to @p push, in the order they were cut.
        ///
        /// @param push Callable `(const StoredLabel&)`.
        template <typename PushCallable>
        void restore_into(const PushCallable& push) {
            for (const auto& stored : aside_) {
                push(stored);
            }
            aside_.clear();
        }

        /// @brief Erases every label stored aside, through @p erase, and releases it.
        ///
        /// @param erase Callable `(const LabelIterator&)` erasing a label from its node.
        template <typename EraseCallable>
        void release(LabelPool<ResourceType>* pool, const EraseCallable& erase) {
            for (const auto& stored : aside_) {
                erase(stored.position);
                // A stored label pins a predecessor and may itself be pinned: releasing through
                // the reference counts keeps the chain balanced.
                pool->release_with_ref_count(stored.label);
            }
            aside_.clear();
        }

        /// @brief Forgets every label stored aside, releasing none (the pool owns them).
        void clear() { aside_.clear(); }

    private:
        std::vector<StoredLabel<ResourceType>> aside_;
};

// ─── First in, first out ────────────────────────────────────────────────────────────────────

/// @brief Hands labels out first in, first out, up to the per-node quota (Simple's order).
///
/// A label taken while dominated is released; one over its node's quota is cut
/// (@ref CutLabels). Either way, @ref next goes on to the next label.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class FifoSchedule {
    public:
        /// @brief Sizes the per-node counts for the graph being solved.
        void initialize(detail::SearchContext<ResourceType>& ctx) {
            quota_.reset(ctx.graph->get_number_of_nodes());
        }

        /// @brief The next label to extend and its position in its node's container; a null
        ///        label when none is left.
        StoredLabel<ResourceType> next(detail::SearchContext<ResourceType>& ctx) {
            while (!queue_.empty()) {
                const StoredLabel<ResourceType> stored = queue_.front();
                queue_.pop_front();

                // A dequeued label still pins the predecessor it was extended from: release it
                // through the reference counts, or that predecessor leaks.
                if (stored.label->dominated) {
                    ctx.pool->release_with_ref_count(stored.label);
                    continue;
                }
                if (quota_.may_take(stored.label->get_end_node()->pos(), *ctx.quota)) {
                    return stored;
                }
                cut_.cut(ctx, stored);
            }
            // Every label taken was released or cut: none to process. The last one must not be
            // handed back, or the caller would release it again or extend a label cut.
            return {};
        }

        /// @brief The labels waiting to be handed out.
        [[nodiscard]] size_t size() const { return queue_.size(); }

        /// @brief Queues a newly kept label.
        void push(const StoredLabel<ResourceType>& stored) { queue_.push_back(stored); }

        /// @brief Resets the per-node counts and queues again what the quota stored aside.
        void prepare_next_phase(detail::SearchContext<ResourceType>& /*ctx*/) {
            quota_.restart();
            cut_.restore_into([this](const StoredLabel<ResourceType>& stored) { push(stored); });
        }

        /// @brief Keeps the queue's @p limit × nodes best labels (non-dominated first, then
        ///        cheapest); of the rest, worst first, a dominated label is released and a
        ///        non-dominated one is cut.
        void trim(detail::SearchContext<ResourceType>& ctx, size_t limit) {
            const size_t max_total = limit * ctx.graph->get_number_of_nodes();
            auto excess = detail::split_off_excess(&queue_, max_total);
            // Worst first, as they come off the back of the queue.
            for (auto it = excess.rbegin(); it != excess.rend(); ++it) {
                if (it->label->dominated) {
                    ctx.pool->release_with_ref_count(it->label);
                } else {
                    cut_.cut(ctx, *it);
                }
            }
        }

        /// @brief Erases (through @p erase) and releases what the cuts stored aside.
        template <typename EraseCallable>
        void release_stored_aside(detail::SearchContext<ResourceType>& ctx,
                                  const EraseCallable& erase) {
            cut_.release(ctx.pool, erase);
        }

        /// @brief Forgets every label, releasing none (the pool owns them), and resets the counts.
        void clear() {
            queue_.clear();
            cut_.clear();
            quota_.restart();
        }

    private:
        std::list<StoredLabel<ResourceType>> queue_;
        CutLabels<ResourceType> cut_;
        PerNodeQuota quota_;
};

}  // namespace rcspp::parts
