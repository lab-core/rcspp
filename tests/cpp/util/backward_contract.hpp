// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"

namespace test_util {

/// @brief Asserts the defining property of a threshold resource's backward extension:
///        extend(x, arc) <= theta   <==>   x <= extend_back(theta, arc)
///
/// Applies to @c rcspp::BackwardKind::Threshold functions only. Precondition: neither the
/// forward clamp (@c earliest[destination]) nor the backward clamp (@c latest[origin]) may bind
/// over the sampled range, since each breaks the biconditional; test clamps with explicit cases.
///
/// @tparam ExtensionFn  The extension function under test.
/// @tparam ResourceType The scalar resource type.
/// @tparam ValueType    The underlying arithmetic value type.
/// @param fn         The extension function, already preprocessed for the arc under test.
/// @param arc_value  The arc's consumption.
/// @param samples    Forward values @c x to probe.
/// @param thetas     Backward thresholds @c theta to probe.
template <typename ExtensionFn, typename ResourceType, typename ValueType>
void check_backward_contract(ExtensionFn& fn, const ResourceType& arc_value,
                             const std::vector<ValueType>& samples,
                             const std::vector<ValueType>& thetas) {
    for (const ValueType theta : thetas) {
        ResourceType inverted;
        ResourceType theta_resource;
        theta_resource.set_value(theta);
        fn.extend_back(theta_resource, arc_value, &inverted);

        for (const ValueType x : samples) {
            std::ostringstream trace;
            trace << "x=" << x << " theta=" << theta << " arc=" << arc_value.get_value()
                  << " extend_back(theta)=" << inverted.get_value();
            SCOPED_TRACE(trace.str());

            ResourceType extended;
            ResourceType x_resource;
            x_resource.set_value(x);
            fn.extend(x_resource, arc_value, &extended);

            const bool forward_fits = extended.get_value() <= theta;
            const bool backward_fits = x <= inverted.get_value();
            EXPECT_EQ(forward_fits, backward_fits);
        }
    }
}

}  // namespace test_util
