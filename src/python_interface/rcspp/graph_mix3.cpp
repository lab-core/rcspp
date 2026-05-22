// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Three-resource and universal mixed graphs.  Compiled as a separate translation
// unit so that these larger template instantiations build in parallel with
// graph.cpp and graph_mix2.cpp.

#include "graph_impl.hpp"

void init_graph_mix3(py::module_& m) {
    // clang-format off

    // ── Triples (3-type) ──────────────────────────────────────────────────────
    BIND_MIX(RealResource, IntResource,     RealSetResource);
    BIND_MIX(RealResource, IntResource,     IntSetResource);
    BIND_MIX(RealResource, IntSetResource,  UIntBitsetResource);

    // ── Universal (all 5 types) ───────────────────────────────────────────────
    BIND_MIX(RealResource, IntResource, RealSetResource, IntSetResource, UIntBitsetResource);

    // clang-format on
}
