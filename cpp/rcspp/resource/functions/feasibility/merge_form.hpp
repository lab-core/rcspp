// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Supplies the disjointness merge test for a container resource.
///
/// Two halves may join only if their stored sets do not overlap. This is exact only when the
/// model forbids revisits and the stored set is the one the label carries out of its node (a
/// memory that never forgets, or one already narrowed on arrival). Otherwise the join is stricter
/// than the search and results depend on the half-way point, so declare it only where that holds.
///
/// @note @c override, not @c final: @c IntersectionFeasibilityFunction narrows @c merge_rule().
///
/// @tparam R    The container resource type. Must expose `intersects()`.
/// @tparam Base The base to insert above -- @c FeasibilityFunction<R> in every current use.
template <typename R, typename Base>
class DisjointMergeForm : public Base {
    public:
        /// @brief Two halves may be merged only when their remembered sets do not overlap.
        ///
        /// @param resource      The forward label's value.
        /// @param back_resource The backward label's value.
        /// @return @c true when the two sets are disjoint.
        [[nodiscard]] auto can_be_merged(const R& resource,
                                         const R& back_resource) -> bool override {
            return !resource.intersects(back_resource.get_value());
        }

        /// @brief The merge test is supplied here, so the rule is @c Custom.
        ///
        /// @return @c MergeRule::Custom.
        [[nodiscard]] MergeRule merge_rule() const override { return MergeRule::Custom; }
};

}  // namespace rcspp
