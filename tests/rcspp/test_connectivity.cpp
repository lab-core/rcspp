#include <cassert>
#include <iostream>

#include "rcspp/rcspp.hpp"

using namespace rcspp;

int main() {
    // Build a small graph with 5 nodes and various edges
    Graph<RealResource> g;
    for (size_t i = 0; i < 5; ++i) {  // NOLINT
        g.add_node(i);
    }

    // Edges:
    // 0 -> 1, 1 -> 2, 2 -> 0 (cycle among 0,1,2)
    // 2 -> 3, 3 -> 4
    g.add_arc(0, 1);
    g.add_arc(1, 2);
    g.add_arc(2, 0);
    g.add_arc(2, 3);
    g.add_arc(3, 4);

    ConnectivityMatrix<RealResource> cm(&g);
    cm.compute_bitmatrix();

    // In the cycle 0,1,2 all reach each other
    assert(cm.is_connected(0, 1));
    assert(cm.is_connected(1, 2));
    assert(cm.is_connected(2, 0));

    // Nodes 0,1,2 should reach 3 and 4 (via 2->3->4)
    assert(cm.is_connected(0, 3));
    assert(cm.is_connected(1, 4));

    // 4 should not reach any other node except itself (no outgoing)
    assert(cm.is_connected(4, 4));
    assert(cm.is_connected(0, 4));
    assert(!cm.is_connected(4, 0));
    assert(cm.is_connected(1, 3));
    assert(!cm.is_connected(3, 1));

    std::cout << "Connectivity test passed\n";
    return 0;
}

