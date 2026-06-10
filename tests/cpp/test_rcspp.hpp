#pragma once

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"
#include "vrp/instance_reader.hpp"
#include "vrp_subproblem/vrp_subproblem.hpp"

using namespace rcspp;

template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_vrp_solve(const std::map<size_t, double>& dual_by_id, VRPSubproblem* vrp_subproblem,
                    double optimal_cost) {
    auto solutions = vrp_subproblem->solve<AlgorithmType>(dual_by_id);
    ASSERT_FALSE(solutions.empty());
    EXPECT_NEAR(solutions[0].cost, optimal_cost, 1e-9);
}

template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_rcspp() {
    std::string instance_name = "R101";
    std::string root_dir = file_parent_dir(__FILE__, 3);
    std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    std::string duals_dir = root_dir + "/instances/duals/" + instance_name + "/";

    constexpr double OPTIMAL_COST_ITER_0 = -319.87786809696524415;
    auto dual_by_id = InstanceReader::read_duals(duals_dir + "iter_0.txt");
    ASSERT_NO_FATAL_FAILURE(
        test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_0));

    constexpr double OPTIMAL_COST_ITER_1 = -291.88751273511473983;
    dual_by_id = InstanceReader::read_duals(duals_dir + "iter_1.txt");
    test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_1);
}

template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_rcspp_non_integer_dual_row_coef() {
    std::string instance_name = "R101";
    std::string root_dir = file_parent_dir(__FILE__, 3);
    std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();

    constexpr double DUAL_ROW_COEF = 0.5;
    std::map<size_t, double> coef_by_id;
    for (const auto& [key, value] : instance.get_customers_by_id()) {
        coef_by_id.emplace(key, DUAL_ROW_COEF);
    }
    VRPSubproblem vrp_subproblem(instance, &coef_by_id);

    constexpr double DUAL_COEF = 1.0 / DUAL_ROW_COEF;
    std::string duals_dir = root_dir + "/instances/duals/" + instance_name + "/";

    constexpr double OPTIMAL_COST_ITER_0 = -319.87786809696524;
    auto dual_by_id = InstanceReader::read_duals(duals_dir + "iter_0.txt");
    for (auto& [key, value] : dual_by_id) value *= DUAL_COEF;
    ASSERT_NO_FATAL_FAILURE(
        test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_0));

    constexpr double OPTIMAL_COST_ITER_1 = -291.88751273511473983;
    dual_by_id = InstanceReader::read_duals(duals_dir + "iter_1.txt");
    for (auto& [key, value] : dual_by_id) value *= DUAL_COEF;
    test_vrp_solve<AlgorithmType>(dual_by_id, &vrp_subproblem, OPTIMAL_COST_ITER_1);
}

#define DEFINE_RCSPP_TESTS(AlgoSuffix, AlgoType)          \
    TEST(Rcspp_##AlgoSuffix, Iter0Iter1) {                \
        test_rcspp<AlgoType>();                           \
    }                                                     \
    TEST(Rcspp_##AlgoSuffix, NonIntegerDualRowCoef) {     \
        test_rcspp_non_integer_dual_row_coef<AlgoType>(); \
    }

DEFINE_RCSPP_TESTS(SimpleDominance, SimpleDominanceAlgorithm)
DEFINE_RCSPP_TESTS(PushingDominance, PushingDominanceAlgorithm)
DEFINE_RCSPP_TESTS(PullingDominance, PullingDominanceAlgorithm)
DEFINE_RCSPP_TESTS(AStarDominance, AStarAlgoBound<RealResource>::Algo)

#undef DEFINE_RCSPP_TESTS

// ── Memory-limit tests ────────────────────────────────────────────────────────

/// Verify that setting max_memory_bytes = 1 (immediately exceeded on the first
/// check) causes solve() to stop cleanly without crashing or hanging, and
/// returns either an empty result or whatever solutions were found before the
/// limit fired.
template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_memory_limit_immediate_stop() {
    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    const std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    auto dual_by_id =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    // ~1 byte expressed as GiB: always exceeded on the very first check.
    constexpr double kTinyLimitGiB = 1e-9;
    AlgorithmBaseParams base;
    base.max_memory_gb = kTinyLimitGiB;
    base.memory_check_interval = 1;

    // Must not crash or hang.  Solutions may be empty (stopped before any found).
    ASSERT_NO_THROW(vrp_subproblem.solve<AlgorithmType>(dual_by_id, base));
}

/// Verify that enabling limit_to_available_ram causes solve() to complete
/// without crashing regardless of whether the limit is hit.
template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_memory_limit_available_ram() {
    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    const std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    auto dual_by_id =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    // 99 % of available RAM is very generous — the solve should complete normally.
    constexpr double kGenerousLimit = 0.99;
    AlgorithmBaseParams base;
    base.limit_to_available_ram = true;
    base.memory_limit_fraction = kGenerousLimit;

    auto solutions = vrp_subproblem.solve<AlgorithmType>(dual_by_id, base);
    ASSERT_FALSE(solutions.empty());
    constexpr double kOptimal = -319.87786809696524415;
    EXPECT_NEAR(solutions[0].cost, kOptimal, 1e-9);
}

/// Verify that memory-pressure pruning fires (pressure_fraction = 0 → always
/// triggered) without corrupting results: the optimal solution must still be
/// found when the hard limit is not reached.
template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_memory_pressure_pruning() {
    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    const std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    auto dual_by_id =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    // Trigger pruning on every check (pressure_fraction = 0 → always under pressure)
    // but never stop (max_memory_gb very large → limit never exceeded).
    constexpr size_t kGenerousQueueSize = 10'000;
    constexpr size_t kCheckInterval = 1'000;
    AlgorithmBaseParams base;
    constexpr double kHugeLimitGiB = 1e9;  // 1 billion GiB — effectively unlimited
    base.max_memory_gb = kHugeLimitGiB;
    base.memory_pressure_fraction = 0.0;
    base.memory_check_interval = kCheckInterval;
    base.memory_pressure_max_labels_per_node = kGenerousQueueSize;

    auto solutions = vrp_subproblem.solve<AlgorithmType>(dual_by_id, base);
    // Pruning should not corrupt the result.
    ASSERT_FALSE(solutions.empty());
    constexpr double kOptimal = -319.87786809696524415;
    EXPECT_NEAR(solutions[0].cost, kOptimal, 1e-9);
}

/// Verify that limit_to_total_ram with a very generous fraction finds the
/// optimal solution, and that the explicit kGB unit constant is usable.
template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_memory_limit_total_ram() {
    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    const std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    auto dual_by_id =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    // 99 % of total RAM — always generous enough to complete.
    constexpr double kGenerousLimit = 0.99;
    AlgorithmBaseParams base_frac;
    base_frac.limit_to_total_ram = true;
    base_frac.memory_limit_fraction = kGenerousLimit;
    auto solutions_frac = vrp_subproblem.solve<AlgorithmType>(dual_by_id, base_frac);
    ASSERT_FALSE(solutions_frac.empty());
    constexpr double kOptimal = -319.87786809696524415;
    EXPECT_NEAR(solutions_frac[0].cost, kOptimal, 1e-9);

    // Also verify an explicit GiB limit produces the expected result
    // when the limit is very generous (1 000 GiB >> any realistic RSS).
    constexpr double kVeryLargeLimit = 1000.0;  // GiB
    AlgorithmBaseParams base_abs;
    base_abs.max_memory_gb = kVeryLargeLimit;
    auto solutions_abs = vrp_subproblem.solve<AlgorithmType>(dual_by_id, base_abs);
    ASSERT_FALSE(solutions_abs.empty());
    EXPECT_NEAR(solutions_abs[0].cost, kOptimal, 1e-9);
}

#define DEFINE_MEMORY_LIMIT_TESTS(AlgoSuffix, AlgoType)  \
    TEST(Rcspp_##AlgoSuffix, MemoryLimitImmediateStop) { \
        test_memory_limit_immediate_stop<AlgoType>();    \
    }                                                    \
    TEST(Rcspp_##AlgoSuffix, MemoryLimitAvailableRam) {  \
        test_memory_limit_available_ram<AlgoType>();     \
    }                                                    \
    TEST(Rcspp_##AlgoSuffix, MemoryLimitTotalRam) {      \
        test_memory_limit_total_ram<AlgoType>();         \
    }                                                    \
    TEST(Rcspp_##AlgoSuffix, MemoryPressurePruning) {    \
        test_memory_pressure_pruning<AlgoType>();        \
    }

DEFINE_MEMORY_LIMIT_TESTS(SimpleDominance, SimpleDominanceAlgorithm)
DEFINE_MEMORY_LIMIT_TESTS(PushingDominance, PushingDominanceAlgorithm)
DEFINE_MEMORY_LIMIT_TESTS(PullingDominance, PullingDominanceAlgorithm)
DEFINE_MEMORY_LIMIT_TESTS(AStarDominance, AStarAlgoBound<RealResource>::Algo)

#undef DEFINE_MEMORY_LIMIT_TESTS

// ── Label-pool ref-count consistency ───────────────────────────────────────────
//
// The dominance algorithms must release dominated / truncated labels through
// LabelPool::release_with_ref_count() (not the raw release_label()), so the predecessor's
// ref_count is decremented and a label that a live successor still points to is never
// recycled. After a full solve the pool's prev_label/ref_count chain must therefore be
// internally consistent: every in-use label's ref_count equals the number of in-use
// labels naming it as predecessor, and no in-use label points to a recycled predecessor.
// (PushingDominance routes dominated labels through release_with_ref_count; it is a control.)
template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_ref_count_consistency_after_solve() {
    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    const std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    auto dual_by_id =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    double cost = 0.0;
    const bool consistent =
        vrp_subproblem.solve_and_check_ref_counts<AlgorithmType>(dual_by_id, &cost);

    EXPECT_TRUE(consistent)
        << "label pool prev_label/ref_count bookkeeping is inconsistent after solve: a release "
           "path bypassed LabelPool::release_with_ref_count()";
    // The fix must not change the optimum.
    constexpr double kOptimal = -319.87786809696524415;
    EXPECT_NEAR(cost, kOptimal, 1e-9);
}

#define DEFINE_REFCOUNT_TEST(AlgoSuffix, AlgoType)            \
    TEST(Rcspp_##AlgoSuffix, RefCountConsistencyAfterSolve) { \
        test_ref_count_consistency_after_solve<AlgoType>();   \
    }

DEFINE_REFCOUNT_TEST(SimpleDominance, SimpleDominanceAlgorithm)
DEFINE_REFCOUNT_TEST(PushingDominance, PushingDominanceAlgorithm)
DEFINE_REFCOUNT_TEST(PullingDominance, PullingDominanceAlgorithm)
DEFINE_REFCOUNT_TEST(AStarDominance, AStarAlgoBound<RealResource>::Algo)

#undef DEFINE_REFCOUNT_TEST

// ── Exit-status accuracy ────────────────────────────────────────────────────────
//
// The VRP column-generation loop trusts "pricing found no improving column" as a proof of
// LP-optimality ONLY when the pricing subproblem returns AlgorithmStatus::COMPLETE; any other
// status means the solve was cut short, so optimality is not proven (VRP::solve then warns and
// flags proven_optimal = false). These tests verify solve() reports that status accurately —
// end-to-end through the real pricing solver — the signal column generation relies on. (The
// CG-loop branch itself runs only in the Gurobi-backed VRP build, which CI does not compile.)

template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_solve_status_complete() {
    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    const std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    auto dual_by_id =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    // Default params are fully unbounded (no truncation / phase / solution / time / memory cap),
    // so the labeling runs to exhaustion and the result must be COMPLETE.
    const auto result = vrp_subproblem.solve_result<AlgorithmType>(dual_by_id);
    EXPECT_EQ(result.status, AlgorithmStatus::COMPLETE)
        << "an unbounded solve must report COMPLETE, got '" << result.status_string() << "'";
    ASSERT_FALSE(result.solutions.empty());
    constexpr double kOptimal = -319.87786809696524415;
    EXPECT_NEAR(result.solutions[0].cost, kOptimal, 1e-9);
}

template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_solve_status_memory_limit() {
    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    const std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    auto dual_by_id =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    AlgorithmBaseParams base;
    base.max_memory_gb = 1e-9;  // ~1 byte: exceeded on the very first memory check
    base.memory_check_interval = 1;
    const auto result = vrp_subproblem.solve_result<AlgorithmType>(dual_by_id, base);
    EXPECT_EQ(result.status, AlgorithmStatus::MEMORY_LIMIT)
        << "a tiny memory limit must report MEMORY_LIMIT, got '" << result.status_string() << "'";
}

template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_solve_status_timeout() {
    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    const std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    auto dual_by_id =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    AlgorithmBaseParams base;
    base.timeout_s = 0.0;  // a 0-second budget: exceeded on the first should_stop() check
    const auto result = vrp_subproblem.solve_result<AlgorithmType>(dual_by_id, base);
    EXPECT_EQ(result.status, AlgorithmStatus::TIMEOUT)
        << "a 0-second timeout must report TIMEOUT, got '" << result.status_string() << "'";
}

#define DEFINE_STATUS_TESTS(AlgoSuffix, AlgoType)      \
    TEST(Rcspp_##AlgoSuffix, SolveStatusComplete) {    \
        test_solve_status_complete<AlgoType>();        \
    }                                                  \
    TEST(Rcspp_##AlgoSuffix, SolveStatusMemoryLimit) { \
        test_solve_status_memory_limit<AlgoType>();    \
    }                                                  \
    TEST(Rcspp_##AlgoSuffix, SolveStatusTimeout) {     \
        test_solve_status_timeout<AlgoType>();         \
    }

DEFINE_STATUS_TESTS(SimpleDominance, SimpleDominanceAlgorithm)
DEFINE_STATUS_TESTS(PushingDominance, PushingDominanceAlgorithm)
DEFINE_STATUS_TESTS(PullingDominance, PullingDominanceAlgorithm)
DEFINE_STATUS_TESTS(AStarDominance, AStarAlgoBound<RealResource>::Algo)

#undef DEFINE_STATUS_TESTS

// ── COMPLETE status precedence ──────────────────────────────────────────────────
//
// A finished run (number_of_labels() == 0) must be reported COMPLETE even if a timeout /
// interrupt / memory flag is (re-)true at status-determination time — completion takes precedence
// over the early-stop reasons. We exercise this with a tiny memory limit (so
// memory_limit_.is_exceeded() is true when the status is computed) combined with an enormous
// memory_check_interval (so the periodic check never fires during the solve and the search runs
// to completion). The status must then be COMPLETE, not MEMORY_LIMIT.
template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
void test_status_complete_beats_memory_flag() {
    const std::string instance_name = "R101";
    const std::string root_dir = file_parent_dir(__FILE__, 3);
    const std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    InstanceReader instance_reader(instance_path);
    auto instance = instance_reader.read();
    VRPSubproblem vrp_subproblem(instance);

    auto dual_by_id =
        InstanceReader::read_duals(root_dir + "/instances/duals/" + instance_name + "/iter_0.txt");

    AlgorithmBaseParams base;
    base.max_memory_gb = 1e-9;  // is_exceeded() is true at status-determination time...
    base.memory_check_interval =
        std::numeric_limits<size_t>::max();  // ...but the periodic check never fires during solve
    const auto result = vrp_subproblem.solve_result<AlgorithmType>(dual_by_id, base);
    EXPECT_EQ(result.status, AlgorithmStatus::COMPLETE)
        << "a finished run must report COMPLETE even with the memory flag set at status time, got '"
        << result.status_string() << "'";
    ASSERT_FALSE(result.solutions.empty());
    constexpr double kOptimal = -319.87786809696524415;
    EXPECT_NEAR(result.solutions[0].cost, kOptimal, 1e-9);
}

#define DEFINE_M2_TEST(AlgoSuffix, AlgoType)                  \
    TEST(Rcspp_##AlgoSuffix, StatusCompleteBeatsMemoryFlag) { \
        test_status_complete_beats_memory_flag<AlgoType>();   \
    }
DEFINE_M2_TEST(SimpleDominance, SimpleDominanceAlgorithm)
DEFINE_M2_TEST(PushingDominance, PushingDominanceAlgorithm)
DEFINE_M2_TEST(PullingDominance, PullingDominanceAlgorithm)
DEFINE_M2_TEST(AStarDominance, AStarAlgoBound<RealResource>::Algo)
#undef DEFINE_M2_TEST

// ── A* heuristic fallback on a negative-cost cycle ──────────────────────────────
//
// AStarDominanceAlgorithm seeds f = g + h from a backward Bellman-Ford over the (reduced) cost
// slot. When that slot contains a negative-cost cycle the Bellman-Ford cannot converge and
// throws; the algorithm catches it and disables the heuristic (h = 0); see
// astar_dominance_algorithm.hpp. This builds a graph whose cost slot has a
// negative-cost cycle A<->B (sum -20), made finite by a capacity resource, and checks that A*:
//   (a) does NOT propagate the Bellman-Ford exception (i.e. the catch fires), and
//   (b) still returns the exact optimum found by the plain SimpleDominanceAlgorithm.
//
// Resource 0 (RealResource) is the cost; resource 1 (IntResource) is a hop budget in [0, 3] that
// caps the path length so the negative-cost cycle cannot be traversed forever.
inline std::unique_ptr<ResourceGraph<RealResource, IntResource>> make_negative_cycle_graph() {
    auto graph = std::make_unique<ResourceGraph<RealResource, IntResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    constexpr int kCapacity = 3;
    graph->add_resource<IntResource>(
        std::make_unique<AdditionExtensionFunction<IntResource>>(),
        std::make_unique<MinMaxFeasibilityFunction<IntResource>>(0, kCapacity),
        std::make_unique<TrivialCostFunction<IntResource>>(),
        std::make_unique<ValueDominanceFunction<IntResource>>());

    graph->add_node(0, /*source=*/true);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);

    // {cost, hops}. A->B->A sums to -20: a negative-cost cycle in the cost slot (slot 0).
    graph->add_arc<RealResource, IntResource>({0.0, 1}, 0, 1);    // S -> A
    graph->add_arc<RealResource, IntResource>({-10.0, 1}, 1, 2);  // A -> B
    graph->add_arc<RealResource, IntResource>({-10.0, 1}, 2, 1);  // B -> A (closes the cycle)
    graph->add_arc<RealResource, IntResource>({5.0, 1}, 1, 3);    // A -> T
    graph->add_arc<RealResource, IntResource>({5.0, 1}, 2, 3);    // B -> T
    return graph;
}

template <template <typename, typename> class AlgorithmType>
SolveResult solve_no_preprocess(ResourceGraph<RealResource, IntResource>* graph) {
    // preprocess=false: skip the preprocessing Bellman-Ford / arc removal so the negative-cost
    // cycle is hit only by the A* heuristic seeding (the code path under test), not by
    // preprocessing.
    return graph->solve<AlgorithmType>(AlgorithmBaseParams{},
                                       std::numeric_limits<double>::infinity(),
                                       /*preprocess=*/false);
}

TEST(AStarHeuristic, NegativeCycleFallbackPreservesOptimum) {
    // Reference optimum from the plain label-correcting algorithm (no heuristic).
    auto ref_graph = make_negative_cycle_graph();
    const SolveResult ref = solve_no_preprocess<SimpleDominanceAlgorithm>(ref_graph.get());
    ASSERT_FALSE(ref.solutions.empty());
    EXPECT_EQ(ref.status, AlgorithmStatus::COMPLETE);
    EXPECT_NEAR(ref.solutions[0].cost, -5.0, 1e-9);  // S -> A -> B -> T (3 hops)

    // A* on the same graph: the heuristic Bellman-Ford hits the negative-cost cycle, so the catch
    // must fire (no exception escapes) and the disabled-heuristic search must match the optimum.
    auto astar_graph = make_negative_cycle_graph();
    SolveResult astar;
    ASSERT_NO_THROW(
        { astar = solve_no_preprocess<AStarAlgoBound<RealResource>::Algo>(astar_graph.get()); });
    ASSERT_FALSE(astar.solutions.empty());
    EXPECT_EQ(astar.status, AlgorithmStatus::COMPLETE);
    EXPECT_NEAR(astar.solutions[0].cost, ref.solutions[0].cost, 1e-9);
}
