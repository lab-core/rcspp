// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <iterator>
#include <list>

namespace rcspp::detail {

/// @brief Cuts a queue of waiting labels down to its @p keep best entries, and returns the
///        others. Non-dominated labels count as better than dominated ones, and cheaper labels
///        as better than costlier ones.
///
/// The caller decides what to do with the entries cut. Usually a dominated one is released,
/// and a non-dominated one is stored aside for the next phase (or abandoned, on a
/// bidirectional side).
///
/// @tparam EntryType A queue entry (@c parts::StoredLabel): a label and its position in its
///                   node's container.
/// @param queue The queue. If any entry is cut, it is left sorted, best first.
/// @param keep  How many entries stay.
/// @return The entries cut, best first; empty when @p queue held at most @p keep.
template <typename EntryType>
std::list<EntryType> split_off_excess(std::list<EntryType>* queue, size_t keep) {
    std::list<EntryType> excess;
    if (queue->size() <= keep) {
        return excess;
    }
    queue->sort([](const EntryType& lhs, const EntryType& rhs) {
        if (lhs.label->dominated != rhs.label->dominated) {
            return !lhs.label->dominated;  // non-dominated first
        }
        return lhs.label->get_cost() < rhs.label->get_cost();  // then cheaper first
    });
    excess.splice(excess.end(),
                  *queue,
                  std::next(queue->begin(), static_cast<std::ptrdiff_t>(keep)),
                  queue->end());
    return excess;
}

}  // namespace rcspp::detail
