// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// A program built against an installed rcspp: the umbrella header compiles from the installed
// headers, and a function of the library links.

#include "rcspp/rcspp.hpp"

int main() {
    rcspp::Timer timer(true);
    timer.stop();
    return 0;
}
