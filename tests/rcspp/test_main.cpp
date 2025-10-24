#include "test_rcspp.hpp"
#include "vrp_subproblem/vrp_subproblem.hpp"

#include <iostream>


int main() {

    int all_tests_passed = 1;

    int passed = 0;
    int total = 0;
    if (test_rcspp()) {
        passed++;
    }
    total++;

    std::cout << passed << "/" << total << " tests passed\n";

    if (passed == total) {
        all_tests_passed = 0;
    }

    return all_tests_passed;
}