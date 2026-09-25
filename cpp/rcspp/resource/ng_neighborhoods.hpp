// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rcspp {

/// @brief One revisit in a path: node @ref node seen at positions @ref first and @ref last, with
///        no visit to it in between.
struct NgCycle {
        size_t node = 0;               ///< The node visited twice.
        size_t first = 0;              ///< Position of the earlier visit in the sequence.
        size_t last = 0;               ///< Position of the later visit.
        std::vector<size_t> interior;  ///< The nodes strictly between the two visits, in order.

        /// @brief Nodes strictly inside the cycle: the size ng-growth rules are expressed in.
        [[nodiscard]] size_t length() const { return interior.size(); }
};

/// @brief Every pair of consecutive visits to the same node in @p sequence.
///
/// A node visited three times yields two cycles, one per consecutive pair. Cycles are ordered by
/// the position of their later visit.
///
/// @param sequence Node ids in traversal order.
/// @return The cycles; empty for an elementary sequence.
[[nodiscard]] inline std::vector<NgCycle> find_cycles(std::span<const size_t> sequence) {
    std::vector<NgCycle> cycles;
    std::unordered_map<size_t, size_t> last_seen;
    for (size_t position = 0; position < sequence.size(); ++position) {
        const size_t node = sequence[position];
        if (auto it = last_seen.find(node); it != last_seen.end()) {
            NgCycle cycle{.node = node, .first = it->second, .last = position, .interior = {}};
            cycle.interior.assign(sequence.begin() + static_cast<std::ptrdiff_t>(it->second) + 1,
                                  sequence.begin() + static_cast<std::ptrdiff_t>(position));
            cycles.push_back(std::move(cycle));
        }
        last_seen[node] = position;
    }
    return cycles;
}

/// @brief Whether @p sequence is feasible under the ng-route relaxation with @p neighborhoods.
///
/// Replays `NgPathExtensionFunction` and the ng presets' `IntersectionFeasibilityFunction` exactly.
/// On the arc `a -> b` the memory becomes `(memory u {a}) n (ng(b) u {b})`. Arriving at a node the
/// table names is infeasible when that node is then in the memory: the forbidden set `{b}` the
/// presets derive for every node they are given. A node absent from the table narrows the memory to
/// `{itself}` and has no forbidden set, so it is never infeasible. A column-generation driver uses
/// this to find the master columns a grown memory has made infeasible.
///
/// @tparam E Element type of the neighbourhood sets.
/// @param sequence      Node ids in traversal order.
/// @param neighborhoods Each node's ng-neighbourhood.
/// @return @c true when no step revisits a node its memory still holds.
template <typename E>
[[nodiscard]] bool is_ng_feasible(std::span<const size_t> sequence,
                                  const std::map<size_t, std::set<E>>& neighborhoods) {
    std::set<E> memory;
    for (size_t i = 0; i + 1 < sequence.size(); ++i) {
        const auto arrived = static_cast<E>(sequence[i + 1]);
        memory.insert(static_cast<E>(sequence[i]));
        std::set<E> narrowed;
        const auto it = neighborhoods.find(sequence[i + 1]);
        for (const E& kept : memory) {
            if (kept == arrived || (it != neighborhoods.end() && it->second.contains(kept))) {
                narrowed.insert(kept);
            }
        }
        if (it != neighborhoods.end() && narrowed.contains(arrived)) {
            return false;
        }
        memory = std::move(narrowed);
    }
    return true;
}

}  // namespace rcspp
