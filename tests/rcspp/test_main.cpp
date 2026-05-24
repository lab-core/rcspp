#include "test_main.hpp"
#include "test_graph.hpp"
#include "test_label_buckets.hpp"
#include "test_solution_pool.hpp"
#include "resource/concrete/functions/feasibility/test_intersection_feasibility_function.hpp"
#include "resource/concrete/functions/extension/test_ng_path_extension_function.hpp"

#include <iostream>

#include "rcspp/rcspp.hpp"


int main() {

    int all_tests_passed = 1;

    int passed = 0;
    int total = 0;

    // Test graph creation, graph update and solving the RCSPP
    auto p =
    all_tests_rcspp<SimpleDominanceAlgorithm, PushingDominanceAlgorithm, PullingDominanceAlgorithm>();
    passed += p.first;
    total += p.second;

    // Test graph creation and graph update with non integer dual
    // row coefficients, and solving the RCSPP
    p =
    all_tests_rcspp_non_integer_dual_row_coef<SimpleDominanceAlgorithm, PushingDominanceAlgorithm, PullingDominanceAlgorithm>();
    passed += p.first;
    total += p.second;

    // Test LabelBuckets (BucketLabelList) operations
    p = all_tests_label_buckets();
    passed += p.first;
    total += p.second;

    // Test IntersectionFeasibilityFunction (forbidden / required semantics + preprocess)
    p = all_tests_intersection_feasibility_function();
    passed += p.first;
    total += p.second;

    // Test NgPathExtensionFunction (preprocess loads / resets per-arc neighborhood)
    p = all_tests_ng_path_extension_function();
    passed += p.first;
    total += p.second;

    // Test Graph methods (force_arc)
    p = all_tests_graph();
    passed += p.first;
    total += p.second;

    LOG_INFO(passed, "/", total, " tests passed\n");

    return total - passed;  // return the number of failed tests
}
