// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/label/cost_ordered_label_list.hpp"

namespace rcspp {

/// @brief The per-node label container a search keeps its labels in when none is named:
///        @c CostOrderedLabelList.
///
/// The default of @c Algorithm, @c BidirectionalLabellingAlgorithm, @c ResourceGraph::solve and
/// @c ResourceGraph::create_algorithm, and of the Python graphs' searches (other than the bucket
/// params'). On a model whose dominance promises a cost order (declared, or inferred for
/// @c CompositionDominanceFunction: see @c DominanceFunction::cost_order_tolerance), it skips the
/// dominance tests the costs rule out; on one that promises nothing it tests every pair, as
/// @c LabelList does. Name @c LabelList in the params (@c AlgorithmParams<LabelList<...>>) to keep
/// labels in storing order.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam Direction    The direction its labels are compared in.
template <typename ResourceType, typename Direction = ForwardDirection>
using DefaultLabelContainer = CostOrderedLabelList<ResourceType, Direction>;

}  // namespace rcspp
