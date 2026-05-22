// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Two-resource mixed graphs.  Compiled as a separate translation unit so that
// the heavy template instantiations for each pair can be built in parallel with
// graph.cpp and graph_mix3.cpp.

#include "graph_impl.hpp"

void init_graph_mix2(py::module_& m) {
    // clang-format off

    // ── Pairs (2-type) ────────────────────────────────────────────────────────
    BIND_MIX(RealResource, IntResource);
    BIND_MIX(RealResource, RealSetResource);
    BIND_MIX(RealResource, IntSetResource);
    BIND_MIX(RealResource, UIntBitsetResource);
    BIND_MIX(IntResource,  RealSetResource);
    BIND_MIX(IntResource,  IntSetResource);
    BIND_MIX(IntResource,  UIntBitsetResource);

    // clang-format on
}
