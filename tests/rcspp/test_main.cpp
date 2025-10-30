#include "test_rcspp.hpp"
#include "vrp_subproblem/vrp_subproblem.hpp"

#include <iostream>


int main() {

    int all_tests_passed = 1;

    int passed = 0;
    int total = 0;

    // Test graph creation, graph update and solving the RCSPP
    if (test_rcspp()) {
        passed++;
    }
    total++;

    // Test graph creation and graph update with non integer dual 
    // row coefficients, and solving the RCSPP
    if (test_rcspp_non_integer_dual_row_coef()) {
        passed++;
    }
    total++;

    std::cout << passed << "/" << total << " tests passed\n";

    if (passed == total) {
        all_tests_passed = 0;
    }

    return all_tests_passed;
}