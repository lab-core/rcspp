// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_set>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/bounds.hpp"
#include "rcspp/algorithm/labelling/half_way.hpp"
#include "rcspp/algorithm/labelling/policies.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::detail {

/// @brief What a search is part of: the whole difference between a single-direction search and a
///        side of a bidirectional one.
enum class Role : std::uint8_t {
    /// A single-direction search: a cut stores the label aside; the prune reads the cost.
    Single,
    /// A bidirectional side: a cut abandons the label, and counts it; the prune adds the bound; a
    /// label past the half-way point is kept and not extended.
    BidirectionalSide,
};

/// @brief What every part of a search reads of its solve. Built once per solve
///        (@c Algorithm::make_context); every pointer is the algorithm's member.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
struct SearchContext {
        const Graph<ResourceType>* graph = nullptr;
        LabelPool<ResourceType>* pool = nullptr;
        const AlgorithmBaseParams* params = nullptr;
        double upper_bound = std::numeric_limits<double>::infinity();  ///< the solve's
        double* incumbent = nullptr;  ///< the best cost found so far
        size_t* quota = nullptr;      ///< the per-node quota; memory pressure tightens it
        const std::unordered_set<Solution>* solutions = nullptr;  ///< those recorded so far
        parts::AdmissionContext admission;                        ///< the rule and what it reads
        const NodeBound<ResourceType>* bound = nullptr;           ///< the completion bound
        Role role = Role::Single;
        size_t* abandoned = nullptr;  ///< a side: counts the labels a cut abandons
        const HalfWay<ResourceType>* half_way = nullptr;  ///< a side: where it stops

        /// @brief Whether a cut abandons a label (and the incumbent prune adds the bound): a
        ///        bidirectional side runs one phase, so it cannot store a label aside for the
        ///        next.
        [[nodiscard]] bool abandons() const { return role == Role::BidirectionalSide; }

        /// @brief Whether the solutions recorded so far spend the solve's budget
        ///        (@c max_solutions).
        [[nodiscard]] bool budget_spent() const {
            return solutions->size() >= params->max_solutions;
        }
};

}  // namespace rcspp::detail
