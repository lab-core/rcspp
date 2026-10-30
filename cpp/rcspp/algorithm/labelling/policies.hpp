// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <list>
#include <vector>

#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/graph/arc.hpp"
#include "rcspp/label/label.hpp"

/// @brief The parts a labelling strategy is built from, and the concepts they meet.
///
/// A strategy (@c rcspp::strategy) names its schedule, and may name its admission rule, its
/// retention, its observer and its interleave; each is given the resource type and the search's
/// direction. The library's parts are here and in the schedule headers; a part of one's own meets
/// the same concept.
namespace rcspp::parts {

/// @brief One label and where it is stored at its node.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
struct StoredLabel {
        Label<ResourceType>* label = nullptr;  ///< none: no label
        /// @brief Its position in its node's container.
        typename std::list<Label<ResourceType>*>::iterator position;
};

/// @brief How one extension of a label along an arc ended.
enum class ExtensionOutcome : std::uint8_t {
    Unreachable,  ///< the look-ahead ruled the arc out; no label was drawn
    Infeasible,   ///< the extended label failed its feasibility test and was released
    Dominated,    ///< the admission rule rejected the extended label, which was released
    Accepted,     ///< the extended label was kept and handed on to be extended in turn
};

// ─── Admission ───────────────────────────────────────────────────────────────────────────────

/// @brief The rule that decides whether a new label is kept at its node.
enum class AdmissionRule : std::uint8_t {
    Exact,  ///< dominance: a label dominated at its node is dropped
};

/// @brief What an admission rule reads besides the candidate and its node. Built once per solve
///        from the params.
struct AdmissionContext {
        AdmissionRule rule = AdmissionRule::Exact;
};

/// @brief One node's container, as an admission rule sees it.
///
/// @tparam ContainerType The per-node label container.
template <typename ContainerType>
class NodeView {
    public:
        explicit NodeView(ContainerType* container) : container_(container) {}

        /// @brief Whether a label at the node dominates @p label.
        template <typename ResourceType>
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& label) const {
            return container_->is_dominated(label);
        }

        /// @brief Removes from the node the labels @p label dominates, marking them dominated.
        template <typename ResourceType>
        void evict_dominated_by(const Label<ResourceType>& label) {
            container_->remove_dominated_labels(label);
        }

        /// @brief The labels stored at the node.
        [[nodiscard]] size_t size() const { return container_->get_labels().size(); }

    private:
        ContainerType* container_;
};

/// @brief Whether a candidate label is kept at its node.
///
/// @c admit reads the candidate and the node it would be stored at, through a node view
/// (@ref NodeView): it decides keep or reject, and may evict labels the candidate dominates there,
/// but touches nothing else. @c exact says whether the rule keeps every label exact dominance
/// keeps. It must answer @c true only for such a rule: a search whose admission is not exact is
/// never reported proven.
///
/// @tparam AdmissionType The rule.
/// @tparam ResourceType  The resource type carried by labels.
/// @tparam NodeViewType  The node view it is handed; a list's by default.
template <typename AdmissionType, typename ResourceType,
          typename NodeViewType = NodeView<LabelList<ResourceType>>>
concept Admission = requires(const AdmissionType admission, const Label<ResourceType>& candidate,
                             NodeViewType& node, const AdmissionContext& ctx) {
    { admission.admit(candidate, node, ctx) } -> std::same_as<bool>;
    { AdmissionType::exact(ctx) } -> std::same_as<bool>;
};

/// @brief Dominance, by the rule its context names: a candidate is kept unless a label at its
///        node dominates it, and evicts the labels it dominates there. The rule is chosen at run
///        time, so one instantiation serves every rule; the exact rule is the default.
struct DominanceAdmission {
        /// @brief Whether the rule keeps every label exact dominance keeps.
        [[nodiscard]] static bool exact(const AdmissionContext& ctx) {
            return ctx.rule == AdmissionRule::Exact;
        }

        /// @brief Whether @p candidate is kept at @p node; under the exact rule, the labels it
        ///        dominates there are evicted when it is.
        ///
        /// @param candidate The extended label, feasible and not yet stored.
        /// @param node      The view of the node it would be stored at.
        /// @param ctx       The rule and what it reads.
        /// @return @c true when the candidate is kept.
        template <typename ResourceType, typename NodeViewType>
        bool admit(const Label<ResourceType>& candidate, NodeViewType& node,
                   const AdmissionContext& ctx) const {
            switch (ctx.rule) {
                case AdmissionRule::Exact:
                    if (node.is_dominated(candidate)) {
                        return false;
                    }
                    node.evict_dominated_by(candidate);
                    return true;
            }
            return true;
        }
};

// ─── Retention ───────────────────────────────────────────────────────────────────────────────

/// @brief Where the labels a search keeps live, and what an admission rule may do to a node.
///
/// @c node gives the view an admission rule reads and evicts through; @c store keeps a label at
/// its node and returns its position there; @c erase takes a stored label out.
/// @c collects_at_take says whether solutions are recorded when an end-node label is taken,
/// because nothing stays at the end nodes to read them from at the end.
///
/// @tparam RetentionType The retention.
/// @tparam ResourceType  The resource type carried by labels.
template <typename RetentionType, typename ResourceType>
concept Retention = requires(RetentionType retention, size_t pos, Label<ResourceType>* label,
                             const typename std::list<Label<ResourceType>*>::iterator& position) {
    retention.node(pos);
    {
        retention.store(pos, label)
    } -> std::same_as<typename std::list<Label<ResourceType>*>::iterator>;
    retention.erase(position);
    { RetentionType::collects_at_take } -> std::convertible_to<bool>;
};

/// @brief Kept labels live in their node's container until dominated or the solve ends: every
///        labelling search.
///
/// @tparam ContainerType The per-node label container, comparing labels in the search's
///                       direction.
template <typename ContainerType>
class KeepAtNodes {
    public:
        static constexpr bool collects_at_take = false;  // solutions are read at the end

        /// @brief One empty container per node, each @p prototype's @c copy(), which keeps its
        ///        configuration and direction.
        ///
        /// @param num_nodes The number of node positions.
        /// @param prototype An empty container, configured as the search wants: for a backward
        ///                  search, the params' container turned backward.
        void reset(size_t num_nodes, const ContainerType& prototype) {
            containers_.clear();
            containers_.reserve(num_nodes);
            for (size_t pos = 0; pos < num_nodes; ++pos) {
                containers_.emplace_back(prototype.copy());
            }
        }

        /// @brief Forgets every container, releasing no label (the pool owns them).
        void clear() { containers_.clear(); }

        /// @brief The view of the container at node position @p pos.
        NodeView<ContainerType> node(size_t pos) {
            return NodeView<ContainerType>(&containers_.at(pos));
        }

        /// @brief Stores @p label at node position @p pos.
        ///
        /// @return Its position in the node's container.
        template <typename ResourceType>
        typename std::list<Label<ResourceType>*>::iterator store(size_t pos,
                                                                 Label<ResourceType>* label) {
            return containers_.at(pos).add_label(label);
        }

        /// @brief Takes a stored label out of its node's container.
        template <typename IteratorType>
        void erase(const IteratorType& position) {
            containers_.at((*position)->get_end_node()->pos()).erase_label(position);
        }

        /// @brief The labels stored at node position @p pos.
        [[nodiscard]] const auto& labels_at(size_t pos) const {
            return containers_.at(pos).get_labels();
        }

        /// @brief The per-node containers, by node position.
        [[nodiscard]] std::vector<ContainerType>& containers() { return containers_; }

        /// @brief The per-node containers, by node position.
        [[nodiscard]] const std::vector<ContainerType>& containers() const { return containers_; }

    private:
        std::vector<ContainerType> containers_;
};

// ─── Observer ────────────────────────────────────────────────────────────────────────────────

/// @brief What a search tells of every extension it tries, the unreachable ones included.
///
/// @tparam ObserverType The observer.
/// @tparam ResourceType The resource type carried by labels.
template <typename ObserverType, typename ResourceType>
concept Observer = requires(ObserverType observer, const Label<ResourceType>& label,
                            const Arc<ResourceType>& arc, ExtensionOutcome outcome) {
    observer.on_extension(label, arc, outcome);
};  // NOLINT(whitespace/newline)

/// @brief The default observer: no event is recorded, and the compiler removes the calls. A test
///        observer records them.
struct NoObserver {
        template <typename ResourceType>
        void on_extension(const Label<ResourceType>& /*label*/, const Arc<ResourceType>& /*arc*/,
                          ExtensionOutcome /*outcome*/) {}
};

// ─── Interleave ──────────────────────────────────────────────────────────────────────────────

/// @brief Which side of a bidirectional search works next.
///
/// @c forward_next is handed both sides; each has @c schedule.size(), the labels waiting, and
/// @c work, the arcs it has extended. It answers @c true for the forward side.
///
/// @tparam InterleaveType The interleave.
/// @tparam ForwardType    The forward side.
/// @tparam BackwardType   The backward side.
template <typename InterleaveType, typename ForwardType, typename BackwardType>
concept Interleave = requires(const InterleaveType interleave, const ForwardType& forward,
                              const BackwardType& backward) {
    { interleave.forward_next(forward, backward) } -> std::convertible_to<bool>;
};

/// @brief Takes from whichever side has extended fewer arcs so far, forward on a tie, and from
///        the other side once one has nothing left.
///
/// The sides share no labels, so the order changes neither side's labels, only what exists when
/// a run stops early: a timeout, `max_iterations` or the memory limit then finds both halves
/// partly built, so the join has pairs to test. With the incumbent-based prune on, the sides also
/// share the incumbent, so the order can change which labels are pruned.
///
/// The number of labels waiting would be a poor measure: the backward side holds a single start
/// label until it starts, so choosing by that number would run the whole forward side first.
struct LeastWorkFirst {
        /// @brief Whether the next unit of work is the forward side's.
        ///
        /// @return @c true when the forward side has labels waiting and the backward side has
        ///         none, or has done at least as much work.
        template <typename ForwardType, typename BackwardType>
        [[nodiscard]] bool forward_next(const ForwardType& forward,
                                        const BackwardType& backward) const {
            return forward.schedule.size() > 0 &&
                   (backward.schedule.size() == 0 || forward.work <= backward.work);
        }
};

}  // namespace rcspp::parts
