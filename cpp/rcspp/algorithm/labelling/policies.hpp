// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <list>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/dominance_mask.hpp"
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
    Masked,       ///< the arc mask rules the arc out (a hard arc); no label was drawn
    Unreachable,  ///< the look-ahead ruled the arc out; no label was drawn
    Infeasible,   ///< the extended label failed its feasibility test and was released
    Dominated,    ///< the admission rule rejected the extended label, which was released
    Accepted,     ///< the extended label was kept and handed on to be extended in turn
};

// ─── Admission ───────────────────────────────────────────────────────────────────────────────

/// @brief The rule that decides whether a new label is kept at its node.
enum class AdmissionRule : std::uint8_t {
    Exact,    ///< dominance: a label dominated at its node is dropped
    Relaxed,  ///< dominance over the components a mask leaves in: more labels are dropped
    KBest,    ///< dominance, then at most k labels at each node, the cheapest
};

/// @brief What an admission rule reads besides the candidate and its node. Built once per solve
///        from the params.
struct AdmissionContext {
        /// @brief No limit on the labels a node keeps under @c KBest.
        static constexpr size_t kAllLabels = std::numeric_limits<size_t>::max();

        AdmissionRule rule = AdmissionRule::Exact;
        /// @c Relaxed: the components left out of dominance; none, or a mask that leaves nothing
        /// out: dominance is exact.
        const DominanceMask* dominance_mask = nullptr;
        /// @c KBest: the most labels a node keeps; @ref kAllLabels: every label dominance keeps.
        size_t k = kAllLabels;

        /// @brief The mask @c Relaxed compares over, or none when it would leave nothing out.
        [[nodiscard]] const DominanceMask* relaxed_mask() const {
            return dominance_mask != nullptr && !dominance_mask->empty() ? dominance_mask : nullptr;
        }
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

        /// @brief Whether a label at the node dominates @p label over the components @p mask
        ///        leaves in.
        template <typename ResourceType>
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& label,
                                        const DominanceMask& mask) const {
            return container_->is_dominated(label, mask);
        }

        /// @brief Removes from the node the labels @p label dominates over the components @p mask
        ///        leaves in, marking them dominated.
        template <typename ResourceType>
        void evict_dominated_by(const Label<ResourceType>& label, const DominanceMask& mask) {
            container_->remove_dominated_labels(label, mask);
        }

        /// @brief Removes the node's costliest label, marking it dominated, when it costs more
        ///        than @p cost.
        ///
        /// @return Whether a label was removed.
        bool evict_costliest_above(double cost) { return container_->remove_costliest_above(cost); }

        /// @brief The labels stored at the node.
        [[nodiscard]] size_t size() const { return container_->get_labels().size(); }

    private:
        ContainerType* container_;
};

/// @brief The view of a node where nothing is stored (@ref KeepWhileWaiting): no label there
///        dominates a candidate, and there is nothing to evict.
struct EmptyNodeView {
        template <typename ResourceType>
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& /*label*/) const {
            return false;
        }

        template <typename ResourceType>
        void evict_dominated_by(const Label<ResourceType>& /*label*/) {}

        template <typename ResourceType>
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& /*label*/,
                                        const DominanceMask& /*mask*/) const {
            return false;
        }

        template <typename ResourceType>
        void evict_dominated_by(const Label<ResourceType>& /*label*/,
                                const DominanceMask& /*mask*/) {}

        static bool evict_costliest_above(double /*cost*/) { return false; }

        [[nodiscard]] static size_t size() { return 0; }
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

/// @brief Dominance, by the rule its context names. The rule is chosen at run time, so one
///        instantiation serves every rule; the exact rule is the default.
///
/// - @c Exact: a candidate is kept unless a label at its node dominates it, and evicts the labels
///   it dominates there.
/// - @c Relaxed: the same, over the components the context's mask leaves in.
/// - @c KBest: exact dominance; then, at a node that already holds k labels, the candidate takes
///   the place of the costliest when it costs less, and is dropped otherwise.
struct DominanceAdmission {
        /// @brief Whether the rule keeps every label exact dominance keeps: @c Exact does, and
        ///        the others at their limit (a mask that leaves nothing out, no limit on k).
        [[nodiscard]] static bool exact(const AdmissionContext& ctx) {
            switch (ctx.rule) {
                case AdmissionRule::Exact:
                    return true;
                case AdmissionRule::Relaxed:
                    return ctx.relaxed_mask() == nullptr;
                case AdmissionRule::KBest:
                    return ctx.k == AdmissionContext::kAllLabels;
            }
            return true;
        }

        /// @brief Whether @p candidate is kept at @p node; the labels it dominates there (over
        ///        the components left in, under @c Relaxed) are evicted when it is, and so is the
        ///        costliest when it takes its place under @c KBest.
        ///
        /// @param candidate The extended label, feasible and not yet stored.
        /// @param node      The view of the node it would be stored at.
        /// @param ctx       The rule and what it reads.
        /// @return @c true when the candidate is kept.
        template <typename ResourceType, typename NodeViewType>
        bool admit(const Label<ResourceType>& candidate, NodeViewType& node,
                   const AdmissionContext& ctx) const {
            if (ctx.rule == AdmissionRule::Relaxed) {
                if (const DominanceMask* mask = ctx.relaxed_mask()) {
                    if (node.is_dominated(candidate, *mask)) {
                        return false;
                    }
                    node.evict_dominated_by(candidate, *mask);
                    return true;
                }
            }
            if (node.is_dominated(candidate)) {
                return false;
            }
            node.evict_dominated_by(candidate);
            // A node holds at most k labels, so a candidate that evicted one always finds room:
            // no candidate evicts and is then dropped.
            return ctx.rule != AdmissionRule::KBest || node.size() < ctx.k ||
                   node.evict_costliest_above(candidate.get_cost());
        }
};

/// @brief Keeps every feasible label, comparing none: a dive's admission. It is exact, since it
///        keeps every label exact dominance keeps (and more).
struct KeepAll {
        /// @brief Whether the rule keeps every label exact dominance keeps: always.
        [[nodiscard]] static bool exact(const AdmissionContext& /*ctx*/) { return true; }

        /// @brief Keeps @p candidate.
        template <typename ResourceType, typename NodeViewType>
        bool admit(const Label<ResourceType>& /*candidate*/, NodeViewType& /*node*/,
                   const AdmissionContext& /*ctx*/) const {
            return true;
        }
};

// ─── Retention ───────────────────────────────────────────────────────────────────────────────

/// @brief Where the labels a search keeps live, and what an admission rule may do to a node.
///
/// @c node gives the view an admission rule reads and evicts through; @c store keeps a label at
/// its node and returns its position there; @c erase takes a stored label out.
/// @c collects_at_take says whether solutions are recorded when an end-node label is taken,
/// because nothing stays at the end nodes to read them from at the end. A retention may also say
/// `static constexpr bool releases_after_expansion = true`: the step then releases every label it
/// takes once it has extended it, or recorded it at an end node (@ref releases_after_expansion_v).
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

/// @brief Whether @p RetentionType keeps nothing behind the search: a retention says so with
///        `static constexpr bool releases_after_expansion = true`; one that does not say keeps
///        what it stores.
template <typename RetentionType>
inline constexpr bool releases_after_expansion_v = [] {
    if constexpr (requires { RetentionType::releases_after_expansion; }) {
        return RetentionType::releases_after_expansion;
    } else {
        return false;
    }
}();

/// @brief Kept labels live only while they wait to be taken: a dive's retention, which keeps
///        nothing behind it.
///
/// Nothing is stored at the nodes. A solution is recorded when its end-node label is taken
/// (`collects_at_take`), and the step releases every label it takes once it is done with it
/// (`releases_after_expansion`), through the reference counts: a label lives on while one of the
/// labels extended from it waits, so the path of every waiting label stays whole.
///
/// @tparam ContainerType The per-node label container of the params, which it does not use.
template <typename ContainerType>
class KeepWhileWaiting {
        using Labels =
            std::remove_cvref_t<decltype(std::declval<const ContainerType&>().get_labels())>;

    public:
        static constexpr bool collects_at_take = true;          // nothing is left at the end nodes
        static constexpr bool releases_after_expansion = true;  // nothing is kept behind

        void reset(size_t /*num_nodes*/, const ContainerType& /*prototype*/) {}

        void clear() {}

        /// @brief The view of a node: nothing is stored there.
        EmptyNodeView node(size_t /*pos*/) { return {}; }

        /// @brief Stores nothing: the label lives in the schedule while it waits.
        ///
        /// @return A position that no container holds; @ref erase ignores it.
        template <typename ResourceType>
        typename std::list<Label<ResourceType>*>::iterator store(size_t /*pos*/,
                                                                 Label<ResourceType>* /*label*/) {
            return {};
        }

        /// @brief Erases nothing: no label is stored.
        template <typename IteratorType>
        void erase(const IteratorType& /*position*/) {}

        /// @brief The labels stored at node position @p pos: none.
        [[nodiscard]] const Labels& labels_at(size_t /*pos*/) const { return none_; }

        /// @brief The per-node containers: none.
        [[nodiscard]] const std::vector<ContainerType>& containers() const {
            return no_containers_;
        }

    private:
        Labels none_;
        std::vector<ContainerType> no_containers_;
};

/// @brief A retention template as a descriptor names it, which a strategy's @c retention<R, Dir>
///        member must be: `using retention = parts::Retain<parts::KeepWhileWaiting>;`.
///
/// @tparam RetentionTemplate The retention, a template on the per-node container type.
template <template <typename> class RetentionTemplate>
struct Retain {
        template <typename ContainerType>
        using type = RetentionTemplate<ContainerType>;
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
