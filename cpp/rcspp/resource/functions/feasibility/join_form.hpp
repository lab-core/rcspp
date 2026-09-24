// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Adds a disjointness join test to a set resource: a forward and a backward label join only
///        if their sets share nothing, as an elementary or ng-route path requires.
///
/// Two halves may join only if their stored sets do not overlap. This is exact only when the
/// model forbids revisits and the stored set is the one the label carries out of its node (a
/// memory that never forgets, or one already narrowed on arrival). Otherwise the join is stricter
/// than the search and results depend on the half-way point, so declare it only where that holds.
///
/// @tparam R    The container resource type. Must expose `intersects()`.
/// @tparam Base The base to insert above -- @c FeasibilityFunction<R> in every current use.
template <typename R, typename Base>
class DisjointJoinForm : public Base {
    public:
        /// @brief Two halves may be joined only when their remembered sets do not overlap.
        ///
        /// @param resource      The forward label's value.
        /// @param back_resource The backward label's value.
        /// @return @c true when the two sets are disjoint.
        [[nodiscard]] auto can_be_joined(const R& resource,
                                         const R& back_resource) -> bool override {
            return !resource.intersects(back_resource.get_value());
        }

        /// @brief The join test is supplied here, so the rule is @c Custom.
        ///
        /// @return @c JoinRule::Custom.
        [[nodiscard]] JoinRule join_rule() const override { return JoinRule::Custom; }
};

}  // namespace rcspp
