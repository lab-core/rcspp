// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>
#include <initializer_list>
#include <list>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;

namespace {

// Build a Solution directly with known column cost and rows (rows must be sorted by index).
Solution make_pool_solution(double col_cost, std::vector<Row> rows,
                            std::initializer_list<size_t> arc_ids) {
    Column col;
    col.cost = col_cost;
    col.rows = std::move(rows);

    std::list<size_t> arc_list(arc_ids);
    std::list<size_t> node_list;
    for (size_t i = 0; i <= arc_ids.size(); ++i) {
        node_list.push_back(i);
    }
    // reduced cost placeholder — pool.price() will overwrite it
    return Solution(col_cost, std::move(node_list), std::move(arc_list), std::move(col));
}

}  // namespace

// ─── add / deduplication ────────────────────────────────────────────────────

bool test_pool_add_deduplication() {
    SolutionPool pool;

    auto s1 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});
    auto s2 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});  // identical to s1
    auto s3 = make_pool_solution(7.0, {{1, 1.0L}}, {20, 21});  // distinct

    pool.add(s1);
    pool.add(s2);  // must be rejected (exact duplicate)
    pool.add(s3);

    if (pool.size() != 2) {
        LOG_ERROR("test_pool_add_deduplication: expected size 2, got ", pool.size(), '\n');
        return false;
    }
    return true;
}

bool test_pool_add_batch() {
    SolutionPool pool;
    auto s1 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});
    auto s2 = make_pool_solution(7.0, {{1, 1.0L}}, {20, 21});

    pool.add({s1, s2, s1});  // s1 added twice: second must be rejected

    if (pool.size() != 2) {
        LOG_ERROR("test_pool_add_batch: expected size 2, got ", pool.size(), '\n');
        return false;
    }
    return true;
}

// ─── price ──────────────────────────────────────────────────────────────────

bool test_pool_price_threshold() {
    SolutionPool pool;
    // column.cost=10, rows=[{0,1}]
    // duals=[3]:  rc = 10 - 3 = 7   → positive, not returned
    // duals=[11]: rc = 10 - 11 = -1 → negative, returned
    pool.add(make_pool_solution(10.0, {{0, 1.0L}}, {0, 1}));

    auto r1 = pool.price({3.0});
    if (!r1.empty()) {
        LOG_ERROR("test_pool_price_threshold: expected empty result for rc=7\n");
        return false;
    }

    auto r2 = pool.price({11.0});
    if (r2.size() != 1) {
        LOG_ERROR("test_pool_price_threshold: expected 1 result for rc=-1, got ", r2.size(), '\n');
        return false;
    }
    if (std::abs(r2[0].cost - (-1.0)) > 1e-9) {
        LOG_ERROR("test_pool_price_threshold: wrong reduced cost ", r2[0].cost, '\n');
        return false;
    }
    return true;
}

bool test_pool_price_updates_solution_cost() {
    SolutionPool pool;
    // column.cost=6, rows=[{0,2},{1,1}]
    // duals=[1,2]: rc = 6 - 2*1 - 1*2 = 2
    pool.add(make_pool_solution(6.0, {{0, 2.0L}, {1, 1.0L}}, {0, 1}));

    (void)pool.price({1.0, 2.0});

    auto entries = pool.get_entries();
    if (std::abs(entries[0].first.cost - 2.0) > 1e-9) {
        LOG_ERROR("test_pool_price_updates_solution_cost: cost not updated, got ",
                  entries[0].first.cost, '\n');
        return false;
    }
    return true;
}

bool test_pool_price_out_of_range_dual() {
    SolutionPool pool;
    // rows=[{5, 1.0}] but only 3 duals provided → dual at index 5 treated as 0
    // rc = 8.0 - 0 = 8.0
    pool.add(make_pool_solution(8.0, {{5, 1.0L}}, {0, 1}));

    auto r = pool.price({1.0, 2.0, 3.0});  // only indices 0-2 provided

    auto entries = pool.get_entries();
    if (std::abs(entries[0].first.cost - 8.0) > 1e-9) {
        LOG_ERROR("test_pool_price_out_of_range_dual: expected rc=8, got ",
                  entries[0].first.cost, '\n');
        return false;
    }
    if (!r.empty()) {
        LOG_ERROR("test_pool_price_out_of_range_dual: expected no results (rc=8 > 0)\n");
        return false;
    }
    return true;
}

// ─── activity tracking ───────────────────────────────────────────────────────

bool test_pool_activity_tracking() {
    SolutionPool pool;
    // column.cost=10, rows=[{0,1}]
    pool.add(make_pool_solution(10.0, {{0, 1.0L}}, {0, 1}));

    // Pricing 1: rc = 10 - 3 = 7 → positive
    (void)pool.price({3.0});
    {
        auto e = pool.get_entries();
        const auto& act = e[0].second;
        if (act.age != 1 || act.use_count != 0 || act.last_was_negative) {
            LOG_ERROR("test_pool_activity_tracking: after positive pricing: "
                      "age=", act.age, " use_count=", act.use_count,
                      " last_was_negative=", act.last_was_negative, '\n');
            return false;
        }
        if (std::abs(act.last_reduced_cost - 7.0) > 1e-9) {
            LOG_ERROR("test_pool_activity_tracking: wrong last_reduced_cost ", act.last_reduced_cost, '\n');
            return false;
        }
    }

    // Pricing 2: rc = 10 - 11 = -1 → negative
    (void)pool.price({11.0});
    {
        auto e = pool.get_entries();
        const auto& act = e[0].second;
        if (act.age != 0 || act.use_count != 1 || !act.last_was_negative) {
            LOG_ERROR("test_pool_activity_tracking: after negative pricing: "
                      "age=", act.age, " use_count=", act.use_count,
                      " last_was_negative=", act.last_was_negative, '\n');
            return false;
        }
    }

    // Pricing 3 & 4: two more positive pricings → age = 2
    (void)pool.price({3.0});
    (void)pool.price({3.0});
    {
        auto e = pool.get_entries();
        const auto& act = e[0].second;
        if (act.age != 2 || act.use_count != 1 || act.last_was_negative) {
            LOG_ERROR("test_pool_activity_tracking: after two positive pricings: "
                      "age=", act.age, " use_count=", act.use_count,
                      " last_was_negative=", act.last_was_negative, '\n');
            return false;
        }
    }
    return true;
}

// ─── remove_if ───────────────────────────────────────────────────────────────

bool test_pool_remove_if_by_cost() {
    SolutionPool pool;
    pool.add(make_pool_solution(5.0,  {{0, 1.0L}}, {10, 11}));
    pool.add(make_pool_solution(15.0, {{1, 1.0L}}, {20, 21}));
    pool.add(make_pool_solution(8.0,  {{2, 1.0L}}, {30, 31}));

    // Remove solutions with column.cost > 10
    pool.remove_if([](const Solution& sol, const SolutionActivity&) {
        return sol.column.cost > 10.0;
    });

    if (pool.size() != 2) {
        LOG_ERROR("test_pool_remove_if_by_cost: expected 2, got ", pool.size(), '\n');
        return false;
    }
    for (const auto& [sol, act] : pool.get_entries()) {
        if (sol.column.cost > 10.0) {
            LOG_ERROR("test_pool_remove_if_by_cost: solution with cost ", sol.column.cost,
                      " should have been removed\n");
            return false;
        }
    }
    return true;
}

bool test_pool_remove_if_by_age() {
    SolutionPool pool;
    // s1: column.cost=5, duals=[6] → rc=-1 (below threshold, age reset)
    // s2: column.cost=8, duals=[6] → rc=2  (above threshold, age++)
    pool.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    pool.add(make_pool_solution(8.0, {{0, 1.0L}}, {20, 21}));

    (void)pool.price({6.0});  // s1: age=0, s2: age=1
    (void)pool.price({6.0});  // s1: age=0, s2: age=2

    // Remove entries with age > 1
    pool.remove_if([](const Solution&, const SolutionActivity& act) {
        return act.age > 1;
    });

    if (pool.size() != 1) {
        LOG_ERROR("test_pool_remove_if_by_age: expected 1 entry, got ", pool.size(), '\n');
        return false;
    }
    if (std::abs(pool.get_entries()[0].first.column.cost - 5.0) > 1e-9) {
        LOG_ERROR("test_pool_remove_if_by_age: wrong solution kept\n");
        return false;
    }
    return true;
}

bool test_pool_remove_convenience() {
    SolutionPool pool;
    pool.add(make_pool_solution(5.0,  {{0, 1.0L}}, {10, 11}));
    pool.add(make_pool_solution(20.0, {{0, 1.0L}}, {20, 21}));

    // Age the first solution 3 times (positive pricing)
    (void)pool.price({0.0});
    (void)pool.price({0.0});
    (void)pool.price({0.0});

    // remove(max_age=2, max_cost=15): s1 has age=3 (>2) → removed; s2 has cost=20 (>15) → removed
    pool.remove(2, 15.0);

    if (pool.size() != 0) {
        LOG_ERROR("test_pool_remove_convenience: expected empty pool, got ", pool.size(), '\n');
        return false;
    }
    return true;
}

bool test_pool_remove_preserves_index_consistency() {
    SolutionPool pool;
    pool.add(make_pool_solution(5.0,  {{0, 1.0L}}, {10, 11}));
    pool.add(make_pool_solution(8.0,  {{1, 1.0L}}, {20, 21}));
    pool.add(make_pool_solution(12.0, {{2, 1.0L}}, {30, 31}));

    // Remove middle entry
    pool.remove_if([](const Solution& sol, const SolutionActivity&) {
        return sol.column.cost == 8.0;
    });

    if (pool.size() != 2) {
        LOG_ERROR("test_pool_remove_preserves_index_consistency: expected 2, got ", pool.size(), '\n');
        return false;
    }

    // Re-add the removed solution — must not be treated as duplicate of existing ones
    pool.add(make_pool_solution(8.0, {{1, 1.0L}}, {20, 21}));
    if (pool.size() != 3) {
        LOG_ERROR("test_pool_remove_preserves_index_consistency: re-add after remove failed, "
                  "size=", pool.size(), '\n');
        return false;
    }
    return true;
}

// ─── runner ─────────────────────────────────────────────────────────────────

std::pair<int, int> all_tests_solution_pool() {
    int passed = 0;
    int total = 0;

    auto run = [&](bool (*fn)(), const char* name) {
        LOG_INFO("Run test ", name, '\n');
        ++total;
        if (fn()) {
            ++passed;
        } else {
            LOG_ERROR("FAILED: ", name, '\n');
        }
    };

    run(test_pool_add_deduplication,                "test_pool_add_deduplication");
    run(test_pool_add_batch,                        "test_pool_add_batch");
    run(test_pool_price_threshold,                  "test_pool_price_threshold");
    run(test_pool_price_updates_solution_cost,      "test_pool_price_updates_solution_cost");
    run(test_pool_price_out_of_range_dual,          "test_pool_price_out_of_range_dual");
    run(test_pool_activity_tracking,                "test_pool_activity_tracking");
    run(test_pool_remove_if_by_cost,                "test_pool_remove_if_by_cost");
    run(test_pool_remove_if_by_age,                 "test_pool_remove_if_by_age");
    run(test_pool_remove_convenience,               "test_pool_remove_convenience");
    run(test_pool_remove_preserves_index_consistency,
        "test_pool_remove_preserves_index_consistency");

    return {passed, total};
}
