// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <map>
#include <random>

namespace rcspp {

/**
 * @brief Reusable tabu-list bookkeeping shared by tabu-style RCSPP algorithms.
 *
 * Tracks arc ids (by @c size_t key) with a remaining tenure (in iterations), an adaptive
 * "extra" tenure that the calling algorithm grows on duplicate solutions and shrinks on
 * novel ones, optional ±1 random jitter on freshly added entries, and a per-iteration
 * @ref age step.
 *
 * This class is consumed by composition: it does not assume how the algorithm dives,
 * filters arcs, or interprets a solution. Two examples in this package:
 *
 *   - @ref TabuSearchAlgorithm consults @ref is_tabu while extending labels and calls
 *     @ref add for each arc on a found path.
 *   - @ref DiversificationSearch removes arcs from a graph clone when calling @ref add
 *     and passes a "restore arc" callback to @ref age so expired arcs come back.
 *
 * The @ref grow_extra / @ref shrink_extra split (rather than a single update) is
 * deliberate: each algorithm decides which heuristic of "what counts as a successful
 * iteration" applies to it.
 */
class TabuList {
    public:
        explicit TabuList(int seed) : rnd_(seed) {}

        /// True iff @p arc_id is currently in the tabu set with positive tenure.
        [[nodiscard]] bool is_tabu(size_t arc_id) const {
            auto it = tenure_.find(arc_id);
            return it != tenure_.end() && it->second > 0;
        }

        /// Insert @p arc_id with tenure = base_tenure + extra (+ optional ±1 noise).
        /// If the arc is already in the list, the larger of the two tenures wins.
        /// Returns the tenure actually assigned this call.
        size_t add(size_t arc_id, size_t base_tenure, bool noise) {
            size_t t = base_tenure + extra_;
            if (noise && t > 0) {
                // Apply the ±1 jitter on the size_t directly: a tenure exceeding INT_MAX cannot be
                // round-tripped through int (that cast is undefined behaviour). delta == -1 is only
                // drawn when t > 1, so t - 1 >= 1 and the subtraction cannot underflow.
                const int delta = std::uniform_int_distribution<int>(t > 1 ? -1 : 0, 1)(rnd_);
                t = (delta < 0) ? t - 1 : t + static_cast<size_t>(delta);
            }
            auto& current = tenure_[arc_id];
            current = std::max<size_t>(current, t);
            return t;
        }

        /// Decrement every tenure; remove expired entries. @p on_expire is called once
        /// for each arc whose tenure just hit zero (e.g. to restore the arc on a graph
        /// clone). Pass a no-op for "just forget the entry".
        template <typename OnExpire>
        void age(OnExpire&& on_expire) {
            for (auto it = tenure_.begin(); it != tenure_.end();) {
                if (it->second <= 1) {
                    on_expire(it->first);
                    it = tenure_.erase(it);
                } else {
                    --(it->second);
                    ++it;
                }
            }
        }

        /// Convenience overload when no expiry callback is needed.
        void age() {
            auto noop = [](size_t expired_arc_id) noexcept { (void)expired_arc_id; };
            age(noop);
        }

        /// Adaptive tenure: grow when an iteration produced nothing useful (a duplicate
        /// solution or a dead-end). Grows by 1 — the same step @ref shrink_extra removes — so the
        /// adaptive component stays bounded by the number of grow calls (i.e. by the iteration
        /// budget). Keeping it bounded matters: a tenure that exceeds the iteration budget would
        /// forbid nearly every arc and collapse the tabu search into greedy/aspiration moves, and
        /// an unbounded @c extra_ would overflow the tenure arithmetic in @ref add.
        void grow_extra() { ++extra_; }

        /// Adaptive tenure: shrink (down to zero) when an iteration produced something
        /// new. Use only if your algorithm wants to relax the list on success; calling
        /// only @ref grow_extra is also a valid policy.
        void shrink_extra() { extra_ = extra_ > 0 ? extra_ - 1 : 0; }

        void clear() {
            tenure_.clear();
            extra_ = 0;
        }

        [[nodiscard]] bool empty() const { return tenure_.empty(); }
        [[nodiscard]] size_t extra() const { return extra_; }

    private:
        std::map<size_t, size_t> tenure_;  // arc_id -> remaining tenure
        size_t extra_{0};
        std::mt19937_64 rnd_;
};

}  // namespace rcspp
