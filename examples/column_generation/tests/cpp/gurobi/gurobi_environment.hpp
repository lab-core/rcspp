// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The Gurobi environment the Gurobi tests share.

#include <gurobi_c++.h>

#include <memory>

namespace rcspp::column_generation::testing {

/// @brief One environment for every test, without output; null when Gurobi has no license on
///        this machine.
inline GRBEnv* gurobi_environment() {
    static const std::unique_ptr<GRBEnv> shared = []() -> std::unique_ptr<GRBEnv> {
        try {
            auto created = std::make_unique<GRBEnv>(true);
            created->set(GRB_IntParam_OutputFlag, 0);
            created->start();
            return created;
        } catch (const GRBException&) {
            return nullptr;
        }
    }();
    return shared.get();
}

}  // namespace rcspp::column_generation::testing
