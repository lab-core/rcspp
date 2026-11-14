// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <iterator>
#include <list>

namespace rcspp::detail {

/// @brief Cuts a queue of labels waiting to be extended down to its @p keep best entries, best
///        meaning non-dominated first, then cheaper first.
///
/// What happens to the entries cut is the caller's: a dominated one is usually released, a
/// non-dominated one stored aside for the next phase, or abandoned on a bidirectional side.
///
/// @tparam EntryType A queue entry (@c parts::StoredLabel): a label and its position in its
///                   node's container.
/// @param queue The queue; sorted, best first, when anything is cut.
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
