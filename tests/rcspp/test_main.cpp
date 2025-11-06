#include "test_main.hpp"

#include <iostream>

#include "rcspp/algorithm/pulling_dominance_algorithm_iterators.hpp"
#include "rcspp/algorithm/pushing_dominance_algorithm_iterators.hpp"



int main() {

    int all_tests_passed = 1;

    int passed = 0;
    int total = 0;

    // Test graph creation, graph update and solving the RCSPP
    auto p =
    all_tests_rcspp<SimpleDominanceAlgorithmIterators, PushingDominanceAlgorithmIterators, PullingDominanceAlgorithmIterators>();
    passed += p.first;
    total += p.second;

    // Test graph creation and graph update with non integer dual 
    // row coefficients, and solving the RCSPP
    p =
    all_tests_rcspp_non_integer_dual_row_coef<SimpleDominanceAlgorithmIterators, PushingDominanceAlgorithmIterators, PullingDominanceAlgorithmIterators>();
    passed += p.first;
    total += p.second;

    std::cout << passed << "/" << total << " tests passed\n";

    if (passed == total) {
        all_tests_passed = 0;
    }

    return all_tests_passed;
}
