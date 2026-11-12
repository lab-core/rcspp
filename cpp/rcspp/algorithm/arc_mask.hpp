// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rcspp {

/// @brief Arcs a search must not use (hard) or should avoid (soft), by arc id.
///
/// A labelling search never extends a label along a hard arc: no label is drawn from the pool for
/// it, as if the arc were not in the graph. A soft arc is extended as any other; a depth-first
/// schedule's child filter reads it (the tabu searches release a child reached through one,
/// unless every child is). The
/// mask never edits the graph: arc ids, and every bound computed on the whole graph, stay valid.
///
/// An arc id the mask was not sized for is open. @ref Algorithm::set_arc_mask hands one to a
/// search.
class ArcMask {
    public:
        /// @brief What the mask says of one arc.
        enum class State : std::uint8_t {
            Open,  ///< the search may use it
            Soft,  ///< the search should avoid it: a child filter decides
            Hard,  ///< the search never uses it
        };

        ArcMask() = default;

        /// @brief Every arc of ids below @p number_of_arc_ids open.
        explicit ArcMask(size_t number_of_arc_ids) { reset(number_of_arc_ids); }

        /// @brief Opens every arc, and sizes the mask for the ids below @p number_of_arc_ids
        ///        (@c Graph::get_arc_id_bound).
        void reset(size_t number_of_arc_ids) {
            state_.assign(number_of_arc_ids, State::Open);
            number_of_masked_arcs_ = 0;
        }

        /// @brief Sets the state of arc @p arc_id.
        ///
        /// @throws std::out_of_range when the mask was not sized for @p arc_id.
        void set(size_t arc_id, State state) {
            State& current = state_.at(arc_id);
            if (current == State::Open && state != State::Open) {
                ++number_of_masked_arcs_;
            } else if (current != State::Open && state == State::Open) {
                --number_of_masked_arcs_;
            }
            current = state;
        }

        /// @brief The state of arc @p arc_id: open when the mask was not sized for it.
        [[nodiscard]] State state(size_t arc_id) const {
            return arc_id < state_.size() ? state_[arc_id] : State::Open;
        }

        /// @brief Whether arc @p arc_id is never to be used.
        [[nodiscard]] bool is_hard(size_t arc_id) const { return state(arc_id) == State::Hard; }

        /// @brief Whether arc @p arc_id is to be avoided.
        [[nodiscard]] bool is_soft(size_t arc_id) const { return state(arc_id) == State::Soft; }

        /// @brief Whether every arc is open.
        [[nodiscard]] bool empty() const { return number_of_masked_arcs_ == 0; }

        /// @brief How many arcs are soft or hard.
        [[nodiscard]] size_t number_of_masked_arcs() const { return number_of_masked_arcs_; }

    private:
        std::vector<State> state_;
        size_t number_of_masked_arcs_ = 0;
};

}  // namespace rcspp
