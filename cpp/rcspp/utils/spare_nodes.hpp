// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <iterator>
#include <list>
#include <optional>

namespace rcspp::detail {

/// @brief The nodes that a group of @c std::list<T> no longer use, kept for their next insertions
///        instead of being freed.
///
/// An element taken out of a list (@ref erase, @ref take_all) is spliced into a list of spare
/// nodes; the next element inserted into any list of the group (@ref insert, @ref push_back) is
/// spliced back from it, with its value overwritten. A list that gains and loses elements again
/// and again, as the queues and the per-node containers of a search do, solve after solve, then
/// allocates a node only when it holds more elements than it ever did.
///
/// The lists stay plain @c std::list<T>: their iterators are the usual ones, and an iterator to an
/// element stays valid until the element is taken out (splicing moves nodes, it neither frees nor
/// copies them). A spare node holds the value of the element it last held, which is never read.
/// The spare nodes are freed with this object, or by @ref release.
///
/// @tparam T The lists' element type.
template <typename T>
class SpareNodes {
    public:
        using list_type = std::list<T>;
        using iterator = typename list_type::iterator;
        using const_iterator = typename list_type::const_iterator;

        /// @brief Inserts @p value into @p list before @p position, in a spare node when one is
        ///        left.
        ///
        /// @return The position of the new element.
        iterator insert(list_type* list, const_iterator position, const T& value) {
            if (!spare_.has_value() || spare_->empty()) {
                return list->insert(position, value);
            }
            const iterator node = spare_->begin();
            *node = value;
            list->splice(position, *spare_, node);
            return node;
        }

        /// @brief Appends @p value to @p list, in a spare node when one is left.
        ///
        /// @return The position of the new element.
        iterator push_back(list_type* list, const T& value) {
            return insert(list, list->cend(), value);
        }

        /// @brief Takes the element at @p position out of @p list, and keeps its node.
        ///
        /// @return The position after it.
        iterator erase(list_type* list, iterator position) {
            const iterator next = std::next(position);
            spares().splice(spares().end(), *list, position);
            return next;
        }

        /// @brief Takes every element out of @p list, and keeps their nodes.
        void take_all(list_type* list) {
            if (!list->empty()) {
                spares().splice(spares().end(), *list);
            }
        }

        /// @brief How many spare nodes are kept.
        [[nodiscard]] size_t size() const { return spare_.has_value() ? spare_->size() : 0; }

        /// @brief Frees the spare nodes.
        void release() { spare_.reset(); }

    private:
        /// @brief The spare nodes, the list made the first time an element is taken out: a group
        ///        that never takes one out allocates nothing for it (a @c std::list may allocate
        ///        its end node).
        list_type& spares() {
            if (!spare_.has_value()) {
                spare_.emplace();
            }
            return *spare_;
        }

        std::optional<list_type> spare_;
};

}  // namespace rcspp::detail
