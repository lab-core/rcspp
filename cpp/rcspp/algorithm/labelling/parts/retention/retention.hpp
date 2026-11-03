// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <list>

#include "rcspp/label/label.hpp"

namespace rcspp::parts {

/// @brief Where the labels a search keeps live, and what an admission rule may do to a node.
///
/// The strategy chooses the retention: whether labels are stored at the nodes at all
/// (@ref KeepAtNodes) or not (@ref KeepWhileWaiting). The user chooses the container, through the
/// params (@c LabelList, @c LabelBuckets): how they are stored. A retention that stores labels owns
/// one container per node, of that type, and forwards to it, so the kernel reaches storage through
/// one interface.
///
/// The search calls:
/// - @c reset(number_of_nodes, prototype) at the start of each solve: one container per node, each
/// a
///   copy of @p prototype;
/// - @c node(pos), the view an admission rule reads and evicts through (@ref NodeView);
/// - @c store(pos, label), which keeps a newly admitted label at its node and returns its position
///   there, and @c erase(position), which takes a stored label out;
/// - @c labels_at(pos), the labels stored at a node: the end nodes' are read as the solutions when
///   the loop ends (@c Search::labels_at_end_nodes);
/// - @c containers(), every node's container (@c Search::print);
/// - @c clear(), which forgets every label, releasing none (@c Search::clear).
///
/// @c records_at_take says whether a solution is recorded when its end-node label is taken,
/// rather than read from the end nodes when the loop ends: a retention that stores nothing at the
/// end nodes must say @c true. A retention may also say
/// `static constexpr bool releases_taken_labels = true`: the step then releases every label it
/// takes once it has extended it, or recorded it at an end node (@ref releases_taken_labels_v).
///
/// @tparam RetentionType The retention.
/// @tparam ResourceType  The resource type carried by labels.
/// @tparam ContainerType The per-node container of the params.
template <typename RetentionType, typename ResourceType, typename ContainerType>
concept Retention = requires(RetentionType retention, const RetentionType const_retention,
                             size_t pos, Label<ResourceType>* label,
                             const typename std::list<Label<ResourceType>*>::iterator& position,
                             const ContainerType& prototype) {
    retention.reset(pos, prototype);
    retention.clear();
    retention.node(pos);
    {
        retention.store(pos, label)
    } -> std::same_as<typename std::list<Label<ResourceType>*>::iterator>;
    retention.erase(position);
    const_retention.labels_at(pos);
    const_retention.containers();
    { RetentionType::records_at_take } -> std::convertible_to<bool>;
};

/// @brief Whether @p RetentionType keeps nothing behind the search: a retention says so with
///        `static constexpr bool releases_taken_labels = true`; one that does not say keeps
///        what it stores.
template <typename RetentionType>
inline constexpr bool releases_taken_labels_v = [] {
    if constexpr (requires { RetentionType::releases_taken_labels; }) {
        return RetentionType::releases_taken_labels;
    } else {
        return false;
    }
}();

/// @brief A retention template as a descriptor names it, which a strategy's @c retention<R, Dir>
///        member must be: `using retention = parts::Retain<parts::KeepWhileWaiting>;`.
///
/// @tparam RetentionTemplate The retention, a template on the per-node container type.
template <template <typename> class RetentionTemplate>
struct Retain {
        template <typename ContainerType>
        using type = RetentionTemplate<ContainerType>;
};

}  // namespace rcspp::parts
