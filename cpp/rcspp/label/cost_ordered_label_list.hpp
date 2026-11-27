// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <list>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/dominance_mask.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/label/label_dominance.hpp"
#include "rcspp/utils/utils.hpp"

namespace rcspp {

/// @brief A node's non-dominated labels in cost order, which lets dominance skip the pairs the
///        costs rule out.
///
/// The labels are kept in a vector of (cost, label) entries sorted by cost, labels of equal cost
/// in the order they were stored: 16 bytes a label, and no list node. When the model's dominance
/// function promises that a dominance implies an order on the costs
/// (@c DominanceFunction::cost_order_tolerance, @c eps), a label is only tested against the
/// stored labels that can dominate it, those costing at most its cost + @c eps, and only removes
/// those it can dominate, from its cost − @c eps on: each scan covers one end of the vector, and
/// reads the costs from it rather than from the labels. Without the promise both scans cover
/// every label, as @c LabelList's do (in cost order, so a scan that finds a dominating label may
/// stop after another number of tests).
///
/// The bounds are one ulp wider than cost ± @c eps, so that rounding them never skips a pair the
/// promise allows.
///
/// Exact: a skipped pair could only have removed a label, so every node keeps the labels
/// @c LabelList keeps, and the search extends the same labels; only the order the labels are kept
/// in differs. A wrong promise keeps labels dominance would have removed: an exact search is
/// slower, never wrong; one that caps the labels a node keeps (@c AdmissionRule::KBest) or the
/// labels it extends (a per-node quota) may keep or extend others.
/// Built with @c RCSPP_VERIFY_COST_ORDER (@c kVerifyCostOrder), the container also tests the pairs
/// it skips, and throws @c std::logic_error, naming the dominance function, on one that is a
/// dominance.
///
/// The promise is read once per solve, from the first label stored after @ref clear (a search's
/// containers are new or cleared at each solve), forward or backward as @p Direction says. A
/// container that keeps its labels in cost order says so (@ref orders_by_cost): the join reads
/// them in that order instead of sorting them, and the solutions at an end node are recorded
/// cheapest first.
///
/// A label is stored at no list position (@ref add_label returns none), so it is erased by the
/// label (@ref erase_label, which @c parts::KeepAtNodes calls with the stored label). A NaN cost
/// is a model error; infinite costs sort last.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam Direction    The direction policy supplying the dominance order.
template <class ResourceType, class Direction = ForwardDirection>
class CostOrderedLabelList final {
        /// One stored label and its cost, read when it was stored.
        struct Entry {
                double cost;
                Label<ResourceType>* label;
        };

    public:
        /// The labels are kept, and read, in cost order.
        static constexpr bool orders_by_cost = true;

        /// @brief The stored labels, cheapest first: what @ref get_labels returns. A view of the
        ///        container, valid until it changes; a default one is empty.
        class LabelView {
            public:
                /// @brief An iterator over the labels, as @c Label pointers.
                class Iterator {
                    public:
                        using iterator_category = std::input_iterator_tag;
                        using iterator_concept = std::forward_iterator_tag;
                        using value_type = Label<ResourceType>*;
                        using difference_type = std::ptrdiff_t;
                        using pointer = void;
                        using reference = Label<ResourceType>*;

                        Iterator() = default;
                        explicit Iterator(typename std::vector<Entry>::const_iterator it)
                            : it_(it) {}

                        reference operator*() const { return it_->label; }
                        Iterator& operator++() {
                            ++it_;
                            return *this;
                        }
                        Iterator operator++(int) {
                            Iterator before = *this;
                            ++it_;
                            return before;
                        }
                        bool operator==(const Iterator& other) const { return it_ == other.it_; }

                    private:
                        typename std::vector<Entry>::const_iterator it_;
                };

                LabelView() = default;
                explicit LabelView(const std::vector<Entry>* entries) : entries_(entries) {}

                [[nodiscard]] Iterator begin() const {
                    return entries_ != nullptr ? Iterator(entries_->begin()) : Iterator();
                }
                [[nodiscard]] Iterator end() const {
                    return entries_ != nullptr ? Iterator(entries_->end()) : Iterator();
                }
                [[nodiscard]] size_t size() const {
                    return entries_ != nullptr ? entries_->size() : 0;
                }
                [[nodiscard]] bool empty() const { return size() == 0; }
                /// @brief The cheapest label; the view must not be empty.
                [[nodiscard]] Label<ResourceType>* front() const { return entries_->front().label; }

            private:
                const std::vector<Entry>* entries_ = nullptr;
        };

        CostOrderedLabelList() = default;

        /// @brief Returns an empty container with the same configuration.
        [[nodiscard]] CostOrderedLabelList copy() const { return CostOrderedLabelList(); }

        /// @brief Returns an empty container that compares labels in direction
        ///        @p TargetDirection, and reads that direction's promise.
        ///
        /// @tparam TargetDirection The direction policy of the copy.
        template <class TargetDirection>
        [[nodiscard]] CostOrderedLabelList<ResourceType, TargetDirection> with_direction() const {
            return CostOrderedLabelList<ResourceType, TargetDirection>();
        }

        /// @brief The stored labels, cheapest first (@ref LabelView).
        [[nodiscard]] LabelView get_labels() const { return LabelView(&entries_); }

        /// @brief Stores @p label at its cost, after the labels of equal cost; the first label
        ///        stored after @ref clear reads the dominance function's promise.
        ///
        /// @return No position: a label here is erased by the label (@ref erase_label).
        typename std::list<Label<ResourceType>*>::iterator add_label(Label<ResourceType>* label) {
            const double cost = label->get_cost();
            assert(!std::isnan(cost) &&
                   "a label's cost is NaN: the model's cost function is wrong");
            if (!tolerance_read_) {
                read_tolerance(*label);
            }
            entries_.insert(std::ranges::upper_bound(entries_, cost, {}, &Entry::cost),
                            Entry{.cost = cost, .label = label});
            return {};
        }

        /// @brief Removes the stored label @p stored names, found by its cost, then among the
        ///        labels of that cost. The label must be stored here, as @c LabelList requires of
        ///        a position: a stored label's cost never changes, so it is found at its cost.
        ///
        /// @param stored The label (@c parts::KeepAtNodes passes the stored label).
        void erase_label(const parts::StoredLabel<ResourceType>& stored) {
            const auto [first, last] =
                std::ranges::equal_range(entries_, stored.label->get_cost(), {}, &Entry::cost);
            const auto it = std::find_if(first, last, [&stored](const Entry& entry) {
                return entry.label == stored.label;
            });
            assert(it != last && "erase_label: the label is not stored in this container");
            if (it != last) {
                entries_.erase(it);
            }
        }

        /// @brief Removes every label, keeping the vector's capacity for the labels to come; the
        ///        next label stored reads the promise again. The container is then as @ref copy
        ///        would make it.
        void clear() {
            entries_.clear();
            tolerance_read_ = false;
            tolerance_.reset();
        }

        /// @brief Logs all stored labels at TRACE level, cheapest first.
        void print_labels() const {
            if (LOG_TRACE_ACTIVE()) {
                for (const Entry& entry : entries_) {
                    LOG_TRACE("  ",
                              entry.label,
                              ": ",
                              entry.label->get_resource().to_string(),
                              "\n");
                }
            }
        }

        /// @brief Marks and removes the stored labels @p label dominates; with a promise, only
        ///        those costing at least its cost − @c eps are tested.
        /// @return Number of labels removed.
        size_t remove_dominated_labels(const Label<ResourceType>& label) {
            auto first = entries_.begin();
            if (tolerance_.has_value()) {
                // One ulp wider than cost - eps, so that rounding the bound never skips a label
                // the promise allows.
                const double low = std::nextafter(label.get_cost() - *tolerance_,
                                                  -std::numeric_limits<double>::infinity());
                first = std::ranges::lower_bound(entries_, low, {}, &Entry::cost);
                if constexpr (kVerifyCostOrder) {
                    for (auto it = entries_.begin(); it != first; ++it) {
                        if (it->label != &label &&
                            detail::label_dominates<Direction>(label, *it->label)) {
                            throw_broken_promise(label, *it->label);
                        }
                    }
                }
            }
            return remove_from(first, label, [](const auto& lhs, const auto& rhs) {
                return detail::label_dominates<Direction>(lhs, rhs);
            });
        }

        /// @brief Marks and removes the stored labels @p label dominates over the components
        ///        @p mask leaves in. A mask may leave out the cost, which the promise is about, so
        ///        every stored label is tested.
        /// @return Number of labels removed.
        size_t remove_dominated_labels(const Label<ResourceType>& label,
                                       const DominanceMask& mask) {
            return remove_from(entries_.begin(), label, [&mask](const auto& lhs, const auto& rhs) {
                return detail::label_dominates<Direction>(lhs, rhs, mask);
            });
        }

        /// @brief Whether a stored label dominates @p label; with a promise, only those costing
        ///        at most its cost + @c eps are tested.
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& label) const {
            // One ulp wider than cost + eps, so that rounding the bound never skips a label the
            // promise allows.
            const double high = tolerance_.has_value()
                                    ? std::nextafter(label.get_cost() + *tolerance_,
                                                     std::numeric_limits<double>::infinity())
                                    : std::numeric_limits<double>::infinity();
            for (auto it = entries_.begin(); it != entries_.end(); ++it) {
                if (it->cost > high) {
                    if constexpr (kVerifyCostOrder) {
                        for (; it != entries_.end(); ++it) {
                            if (it->label != &label &&
                                detail::label_dominates<Direction>(*it->label, label)) {
                                throw_broken_promise(*it->label, label);
                            }
                        }
                    }
                    return false;
                }
                if (it->label != &label && detail::label_dominates<Direction>(*it->label, label)) {
                    return true;
                }
            }
            return false;
        }

        /// @brief Whether a stored label dominates @p label over the components @p mask leaves
        ///        in. A mask may leave out the cost, so every stored label is tested.
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& label,
                                        const DominanceMask& mask) const {
            return std::ranges::any_of(entries_, [&label, &mask](const Entry& entry) {
                return entry.label != &label &&
                       detail::label_dominates<Direction>(*entry.label, label, mask);
            });
        }

        /// @brief Removes the costliest label when it costs more than @p cost (of labels of equal
        ///        cost, the last stored), and marks it dominated, as an evicted label is, so that a
        ///        schedule still holding it releases it when taken instead of extending it.
        /// @return Whether a label was removed.
        bool remove_costliest_above(double cost) {
            if (entries_.empty() || entries_.back().cost <= cost) {
                return false;
            }
            entries_.back().label->dominated = true;
            entries_.pop_back();
            return true;
        }

    private:
        /// @brief Reads the dominance function's promise, in this container's direction, off
        ///        @p label's resource; a resource that is not a composition promises nothing.
        void read_tolerance(const Label<ResourceType>& label) {
            if constexpr (requires { label.get_resource().cost_order_tolerance(); }) {
                const auto& resource = label.get_resource();
                const std::optional<double> tolerance = Direction::backward
                                                            ? resource.back_cost_order_tolerance()
                                                            : resource.cost_order_tolerance();
                if (tolerance.has_value() && !(*tolerance >= 0.0)) {
                    throw std::invalid_argument(
                        resource.cost_order_source(Direction::backward) +
                        " gives a cost-order tolerance of " + number(*tolerance) +
                        ": a tolerance must be >= 0, or none (std::nullopt)");
                }
                tolerance_ = tolerance;
            }
            tolerance_read_ = true;
        }

        /// @brief @p value as a message shows it: six significant digits, an exponent when
        ///        needed (`1e-09`, not `0.000000`).
        [[nodiscard]] static std::string number(double value) {
            std::ostringstream out;
            out << value;
            return out.str();
        }

        /// @brief Removes, from @p first on, the stored labels @p dominates says @p label
        ///        dominates, marking them, in one stable pass.
        template <typename DominatesCallable>
        size_t remove_from(typename std::vector<Entry>::iterator first,
                           const Label<ResourceType>& label, const DominatesCallable& dominates) {
            auto out = first;
            size_t removed = 0;
            for (auto in = first; in != entries_.end(); ++in) {
                if (in->label != &label && dominates(label, *in->label)) {
                    in->label->dominated = true;
                    ++removed;
                } else {
                    if (out != in) {
                        *out = *in;
                    }
                    ++out;
                }
            }
            entries_.erase(out, entries_.end());
            return removed;
        }

        /// @brief Throws: @p dominating dominates @p dominated although the promise said their
        ///        costs rule it out.
        [[noreturn]] void throw_broken_promise(const Label<ResourceType>& dominating,
                                               const Label<ResourceType>& dominated) const {
            std::string source = "the model's dominance function";
            if constexpr (requires { dominating.get_resource().cost_order_source(true); }) {
                source = dominating.get_resource().cost_order_source(Direction::backward);
            }
            throw std::logic_error(
                source + " gives a cost-order tolerance of " + number(*tolerance_) +
                ", but a label of cost " + number(dominating.get_cost()) +
                " dominates one of cost " + number(dominated.get_cost()) + " (" +
                (Direction::backward ? "check_back_dominance" : "check_dominance") +
                "): a dominance must reject a pair whose first label costs more than the "
                "second's cost + the tolerance");
        }

        std::vector<Entry> entries_;       ///< the labels, by cost, then in storing order
        std::optional<double> tolerance_;  ///< the promise read for this solve, if any
        bool tolerance_read_ = false;      ///< whether a label was stored since the last clear
};

}  // namespace rcspp
