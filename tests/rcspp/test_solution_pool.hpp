// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>
#include <initializer_list>
#include <list>
#include <unordered_map>
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
    return Solution(col_cost, std::move(node_list), std::move(arc_list), std::move(col));
}

}  // namespace

// ─── add / deduplication ────────────────────────────────────────────────────

bool test_pool_add_deduplication() {
    SolutionPool pool;
    auto fp = pool.new_filter();

    auto s1 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});
    auto s2 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});  // identical to s1
    auto s3 = make_pool_solution(7.0, {{1, 1.0L}}, {20, 21});  // distinct

    auto id1 = fp.add(s1);
    auto id2 = fp.add(s2);  // must return same id (exact duplicate)
    auto id3 = fp.add(s3);

    if (id1 != id2) {
        LOG_ERROR("test_pool_add_deduplication: duplicate got different ids: ",
                  id1, " vs ", id2, '\n');
        return false;
    }
    if (id1 == id3 || id1 == SolutionPool::kNoId || id3 == SolutionPool::kNoId) {
        LOG_ERROR("test_pool_add_deduplication: distinct solutions got same id or kNoId\n");
        return false;
    }
    if (fp.size() != 2) {
        LOG_ERROR("test_pool_add_deduplication: expected size 2, got ", fp.size(), '\n');
        return false;
    }
    return true;
}

bool test_pool_add_batch() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto s1 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});
    auto s2 = make_pool_solution(7.0, {{1, 1.0L}}, {20, 21});

    auto ids = fp.add({s1, s2, s1});  // s1 added twice: second must return same id

    if (fp.size() != 2) {
        LOG_ERROR("test_pool_add_batch: expected size 2, got ", fp.size(), '\n');
        return false;
    }
    if (ids.size() != 3 || ids[0] != ids[2]) {
        LOG_ERROR("test_pool_add_batch: duplicate s1 should return same ColumnId\n");
        return false;
    }
    return true;
}

// ─── get ────────────────────────────────────────────────────────────────────

bool test_pool_get_by_id() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto s1 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});
    auto id = fp.add(s1);

    auto retrieved = fp.get(id);
    if (!retrieved) {
        LOG_ERROR("test_pool_get_by_id: get() returned nullopt for valid id\n");
        return false;
    }
    if (std::abs(retrieved->column.cost - 5.0) > 1e-9) {
        LOG_ERROR("test_pool_get_by_id: wrong cost ", retrieved->column.cost, '\n');
        return false;
    }

    auto missing = fp.get(SolutionPool::kNoId);
    if (missing) {
        LOG_ERROR("test_pool_get_by_id: get(kNoId) should return nullopt\n");
        return false;
    }
    return true;
}

// ─── price ──────────────────────────────────────────────────────────────────

bool test_pool_price_threshold() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // column.cost=10, rows=[{0,1}]
    // duals=[3]:  rc = 10 - 3 = 7   → positive, not returned
    // duals=[11]: rc = 10 - 11 = -1 → negative, returned
    auto id = fp.add(make_pool_solution(10.0, {{0, 1.0L}}, {0, 1}));

    auto r1 = fp.price({3.0});
    if (!r1.empty()) {
        LOG_ERROR("test_pool_price_threshold: expected empty result for rc=7\n");
        return false;
    }

    auto r2 = fp.price({11.0});
    if (r2.size() != 1) {
        LOG_ERROR("test_pool_price_threshold: expected 1 result for rc=-1, got ", r2.size(), '\n');
        return false;
    }
    if (r2[0].id != id) {
        LOG_ERROR("test_pool_price_threshold: wrong ColumnId in result\n");
        return false;
    }
    if (std::abs(r2[0].reduced_cost - (-1.0)) > 1e-9) {
        LOG_ERROR("test_pool_price_threshold: wrong reduced_cost ", r2[0].reduced_cost, '\n');
        return false;
    }
    // solution.cost must NOT be overwritten — still holds original arc cost
    if (std::abs(r2[0].solution->column.cost - 10.0) > 1e-9) {
        LOG_ERROR("test_pool_price_threshold: solution.cost was mutated to ",
                  r2[0].solution->column.cost, " (should stay 10.0)\n");
        return false;
    }
    return true;
}

bool test_pool_price_does_not_mutate_stored_cost() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // column.cost=6, rows=[{0,2},{1,1}]
    // duals=[1,2]: rc = 6 - 2*1 - 1*2 = 2
    fp.add(make_pool_solution(6.0, {{0, 2.0L}, {1, 1.0L}}, {0, 1}));

    (void)fp.price({1.0, 2.0});

    // The stored column.cost must remain 6.0 (not 2.0)
    auto all = fp.get_all();
    if (std::abs(std::get<1>(all[0]).column.cost - 6.0) > 1e-9) {
        LOG_ERROR("test_pool_price_does_not_mutate_stored_cost: stored cost changed to ",
                  std::get<1>(all[0]).column.cost, " (should stay 6.0)\n");
        return false;
    }
    return true;
}

bool test_pool_price_out_of_range_dual() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // rows=[{5, 1.0}] but only 3 duals provided → dual at index 5 treated as 0
    // rc = 8.0 - 0 = 8.0
    fp.add(make_pool_solution(8.0, {{5, 1.0L}}, {0, 1}));

    auto r = fp.price({1.0, 2.0, 3.0});  // only indices 0-2 provided

    if (!r.empty()) {
        LOG_ERROR("test_pool_price_out_of_range_dual: expected no results (rc=8 > 0)\n");
        return false;
    }
    // Verify stored cost unchanged
    auto all = fp.get_all();
    if (std::abs(std::get<1>(all[0]).column.cost - 8.0) > 1e-9) {
        LOG_ERROR("test_pool_price_out_of_range_dual: stored cost changed\n");
        return false;
    }
    return true;
}

// ─── pool-managed activity tracking ─────────────────────────────────────────

bool test_pool_activity_tracking() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // column.cost=10, rows=[{0,1}]
    // duals=[3]:  rc=7  (positive, age++)
    // duals=[11]: rc=-1 (negative, age=0, use_count++)
    auto id = fp.add(make_pool_solution(10.0, {{0, 1.0L}}, {0, 1}));

    if (fp.pricing_count() != 0) {
        LOG_ERROR("test_pool_activity_tracking: pricing_count should start at 0\n");
        return false;
    }

    {
        auto act = fp.get_activity(id);
        if (!act || act->created_at != 0) {
            LOG_ERROR("test_pool_activity_tracking: created_at should be 0\n");
            return false;
        }
    }

    (void)fp.price({3.0});
    {
        auto act = fp.get_activity(id);
        if (!act || act->age != 1 || act->use_count != 0 || act->last_was_negative) {
            LOG_ERROR("test_pool_activity_tracking: after positive pricing: age=",
                      act->age, " use_count=", act->use_count, '\n');
            return false;
        }
        if (fp.pricing_count() != 1) {
            LOG_ERROR("test_pool_activity_tracking: pricing_count should be 1\n");
            return false;
        }
    }

    (void)fp.price({11.0});
    {
        auto act = fp.get_activity(id);
        if (!act || act->age != 0 || act->use_count != 1 || !act->last_was_negative) {
            LOG_ERROR("test_pool_activity_tracking: after negative pricing: age=",
                      act->age, " use_count=", act->use_count, '\n');
            return false;
        }
        const double rate = act->usage_rate(fp.pricing_count());
        if (std::abs(rate - 0.5) > 1e-9) {
            LOG_ERROR("test_pool_activity_tracking: usage_rate should be 0.5, got ", rate, '\n');
            return false;
        }
    }

    (void)fp.price({3.0});
    (void)fp.price({3.0});
    {
        auto act = fp.get_activity(id);
        if (!act || act->age != 2 || act->use_count != 1) {
            LOG_ERROR("test_pool_activity_tracking: after two positive pricings: age=",
                      act->age, " use_count=", act->use_count, '\n');
            return false;
        }
        const double rate = act->usage_rate(fp.pricing_count());
        if (std::abs(rate - 0.25) > 1e-9) {
            LOG_ERROR("test_pool_activity_tracking: usage_rate should be 0.25, got ", rate, '\n');
            return false;
        }
    }
    return true;
}

// ─── row filters via FilteredSolutionPool ────────────────────────────────────

bool test_pool_price_compulsory_rows() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}, {1, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(3.0, {{2, 1.0L}}, {20, 21}));

    std::vector<double> duals = {10.0, 10.0, 10.0};

    // compulsory_rows={0}: s1 has row 0; s2 does not
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({0}));
        auto r = fp2.price(duals);
        if (r.size() != 1 || r[0].id != id1) {
            LOG_ERROR("test_pool_price_compulsory_rows: expected only s1, got ", r.size(), '\n');
            return false;
        }
    }
    // compulsory_rows={0,1}: only s1 has both
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({0, 1}));
        auto r = fp2.price(duals);
        if (r.size() != 1 || r[0].id != id1) {
            LOG_ERROR("test_pool_price_compulsory_rows: compulsory {0,1}: expected s1, got ",
                      r.size(), '\n');
            return false;
        }
    }
    // compulsory_rows={0,2}: no column has both → empty
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({0, 2}));
        auto r = fp2.price(duals);
        if (!r.empty()) {
            LOG_ERROR("test_pool_price_compulsory_rows: compulsory {0,2}: expected empty, got ",
                      r.size(), '\n');
            return false;
        }
    }
    return true;
}

bool test_pool_price_forbidden_rows() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}, {1, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(3.0, {{2, 1.0L}}, {20, 21}));

    std::vector<double> duals = {10.0, 10.0, 10.0};

    // forbidden_rows={1}: s1 has row 1 → filtered; s2 does not → returned
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {1}));
        auto r = fp2.price(duals);
        if (r.size() != 1 || r[0].id != id2) {
            LOG_ERROR("test_pool_price_forbidden_rows: expected only s2, got ", r.size(), '\n');
            return false;
        }
    }
    // forbidden_rows={0,2}: s1 (row 0) and s2 (row 2) both filtered → empty
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {0, 2}));
        auto r = fp2.price(duals);
        if (!r.empty()) {
            LOG_ERROR("test_pool_price_forbidden_rows: expected empty, got ", r.size(), '\n');
            return false;
        }
    }
    return true;
}

bool test_pool_price_combined_filters() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    fp.add(make_pool_solution(5.0, {{0, 1.0L}, {1, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(3.0, {{0, 1.0L}}, {20, 21}));
    fp.add(make_pool_solution(4.0, {{1, 1.0L}, {2, 1.0L}}, {30, 31}));

    std::vector<double> duals = {10.0, 10.0, 10.0};

    // compulsory={0}, forbidden={1}: must have row 0 AND NOT row 1 → only s2
    auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({0}, {1}));
    auto r = fp2.price(duals);
    if (r.size() != 1 || r[0].id != id2) {
        LOG_ERROR("test_pool_price_combined_filters: expected only s2, got ", r.size(), '\n');
        return false;
    }
    return true;
}

// ─── external variable association ──────────────────────────────────────────

bool test_pool_external_variable_association() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));

    std::unordered_map<SolutionPool::ColumnId, int> master1_vars;
    std::unordered_map<SolutionPool::ColumnId, int> master2_vars;

    master1_vars[id1] = 100;
    master1_vars[id2] = 101;
    master2_vars[id1] = 200;

    auto priced = fp.price({10.0, 10.0});
    for (const auto& pc : priced) {
        bool m1_has = master1_vars.contains(pc.id);
        if (!m1_has) {
            LOG_ERROR("test_pool_external_variable_association: master1 missing var for id ",
                      pc.id, '\n');
            return false;
        }
    }

    for (const auto& pc : priced) {
        if (!master2_vars.contains(pc.id)) {
            master2_vars[pc.id] = 300;
        }
    }
    if (!master2_vars.contains(id2) || master2_vars[id2] != 300) {
        LOG_ERROR("test_pool_external_variable_association: master2 failed to add new column\n");
        return false;
    }
    return true;
}

// ─── remove_if ───────────────────────────────────────────────────────────────

bool test_pool_remove_if_by_cost() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    fp.add(make_pool_solution(5.0,  {{0, 1.0L}}, {10, 11}));
    fp.add(make_pool_solution(15.0, {{1, 1.0L}}, {20, 21}));
    fp.add(make_pool_solution(8.0,  {{2, 1.0L}}, {30, 31}));

    auto removed_ids = fp.global_remove_if(
        [](SolutionPool::ColumnId, const Solution& sol, const ColumnActivity&) {
            return sol.column.cost > 10.0;
        });

    if (removed_ids.size() != 1) {
        LOG_ERROR("test_pool_remove_if_by_cost: expected 1 removed, got ", removed_ids.size(), '\n');
        return false;
    }
    if (fp.size() != 2) {
        LOG_ERROR("test_pool_remove_if_by_cost: expected 2, got ", fp.size(), '\n');
        return false;
    }
    for (const auto& [col_id, sol, act] : fp.get_all()) {
        if (sol.column.cost > 10.0) {
            LOG_ERROR("test_pool_remove_if_by_cost: solution with cost ", sol.column.cost,
                      " should have been removed\n");
            return false;
        }
    }
    return true;
}

// ─── remove_stale ────────────────────────────────────────────────────────────

bool test_pool_remove_stale_by_age() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(8.0, {{0, 1.0L}}, {20, 21}));

    (void)fp.price({6.0});  // s1: age=0, s2: age=1
    (void)fp.price({6.0});  // s1: age=0, s2: age=2

    auto removed_ids = fp.global_remove_stale(1);

    if (removed_ids.size() != 1 || removed_ids[0] != id2) {
        LOG_ERROR("test_pool_remove_stale_by_age: expected id2 removed\n");
        return false;
    }
    if (fp.size() != 1) {
        LOG_ERROR("test_pool_remove_stale_by_age: expected 1 entry, got ", fp.size(), '\n');
        return false;
    }
    auto all = fp.get_all();
    if (std::abs(std::get<1>(all[0]).column.cost - 5.0) > 1e-9) {
        LOG_ERROR("test_pool_remove_stale_by_age: wrong solution kept\n");
        return false;
    }
    return true;
}

bool test_pool_remove_stale_by_usage_rate() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(3.0,   {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(100.0, {{1, 1.0L}}, {20, 21}));

    (void)fp.price({5.0, 0.0});
    (void)fp.price({5.0, 0.0});
    (void)fp.price({5.0, 0.0});
    (void)fp.price({5.0, 0.0});

    // s1: use_count=4, usage_rate=1.0 → kept; s2: use_count=0, usage_rate=0.0 < 0.1 → removed
    auto removed_ids = fp.global_remove_stale(/*max_age=*/100, /*min_usage_rate=*/0.1);

    if (removed_ids.size() != 1 || removed_ids[0] != id2) {
        LOG_ERROR("test_pool_remove_stale_by_usage_rate: expected id2 removed\n");
        return false;
    }
    if (fp.size() != 1) {
        LOG_ERROR("test_pool_remove_stale_by_usage_rate: expected 1, got ", fp.size(), '\n');
        return false;
    }
    if (!fp.get(id1)) {
        LOG_ERROR("test_pool_remove_stale_by_usage_rate: id1 should still be present\n");
        return false;
    }
    return true;
}

bool test_pool_remove_preserves_id_consistency() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0,  {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(8.0,  {{1, 1.0L}}, {20, 21}));
    auto id3 = fp.add(make_pool_solution(12.0, {{2, 1.0L}}, {30, 31}));

    fp.global_remove_if([&](SolutionPool::ColumnId cid, const Solution&, const ColumnActivity&) {
        return cid == id2;
    });

    if (fp.size() != 2) {
        LOG_ERROR("test_pool_remove_preserves_id_consistency: expected 2, got ", fp.size(), '\n');
        return false;
    }
    if (!fp.get(id1) || !fp.get(id3)) {
        LOG_ERROR("test_pool_remove_preserves_id_consistency: id1 or id3 missing after remove\n");
        return false;
    }
    if (fp.get(id2)) {
        LOG_ERROR("test_pool_remove_preserves_id_consistency: id2 should be gone\n");
        return false;
    }
    auto new_id = fp.add(make_pool_solution(8.0, {{1, 1.0L}}, {20, 21}));
    if (fp.size() != 3) {
        LOG_ERROR("test_pool_remove_preserves_id_consistency: re-add after remove failed, "
                  "size=", fp.size(), '\n');
        return false;
    }
    if (new_id == id2) {
        LOG_ERROR("test_pool_remove_preserves_id_consistency: re-added entry reused old id\n");
        return false;
    }
    return true;
}

// ─── update_activity (LP basis membership) ───────────────────────────────────

bool test_pool_update_activity() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0,  {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(8.0,  {{1, 1.0L}}, {20, 21}));

    fp.update_activity({id1});  // id1 in basis, id2 not

    {
        auto act1 = fp.get_activity(id1);
        auto act2 = fp.get_activity(id2);
        if (!act1 || !act2) {
            LOG_ERROR("test_pool_update_activity: get_activity returned nullopt\n");
            return false;
        }
        if (act1->age != 0 || act1->use_count != 1 || !act1->last_was_negative) {
            LOG_ERROR("test_pool_update_activity: id1 in basis: age=", act1->age,
                      " use_count=", act1->use_count, '\n');
            return false;
        }
        if (act2->age != 1 || act2->use_count != 0 || act2->last_was_negative) {
            LOG_ERROR("test_pool_update_activity: id2 not in basis: age=", act2->age,
                      " use_count=", act2->use_count, '\n');
            return false;
        }
    }

    fp.update_activity({});
    {
        auto act1 = fp.get_activity(id1);
        auto act2 = fp.get_activity(id2);
        if (!act1 || act1->age != 1 || act1->use_count != 1) {
            LOG_ERROR("test_pool_update_activity: id1 after second update: age=",
                      act1->age, '\n');
            return false;
        }
        if (!act2 || act2->age != 2 || act2->use_count != 0) {
            LOG_ERROR("test_pool_update_activity: id2 after second update: age=",
                      act2->age, '\n');
            return false;
        }
    }
    if (fp.pricing_count() != 0) {
        LOG_ERROR("test_pool_update_activity: pricing_count should remain 0\n");
        return false;
    }
    return true;
}

// ─── arc-based filters via FilteredSolutionPool ──────────────────────────────

bool test_pool_price_arc_filters() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}, {1, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(3.0, {{0, 1.0L}, {1, 1.0L}}, {20, 21}));

    std::vector<double> duals = {10.0, 10.0};

    // compulsory_arc_ids={10}: only s1 has arc 10 in path
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {10}));
        auto r = fp2.price(duals);
        if (r.size() != 1 || r[0].id != id1) {
            LOG_ERROR("test_pool_price_arc_filters: compulsory arc 10: expected s1, got ",
                      r.size(), '\n');
            return false;
        }
    }

    // forbidden_arc_ids={10}: s1 filtered; only s2 returned
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));
        auto r = fp2.price(duals);
        if (r.size() != 1 || r[0].id != id2) {
            LOG_ERROR("test_pool_price_arc_filters: forbidden arc 10: expected s2, got ",
                      r.size(), '\n');
            return false;
        }
    }

    // forbidden_arc_ids={10,20}: both paths filtered → empty
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10, 20}));
        auto r = fp2.price(duals);
        if (!r.empty()) {
            LOG_ERROR("test_pool_price_arc_filters: forbidden {10,20}: expected empty, got ",
                      r.size(), '\n');
            return false;
        }
    }
    return true;
}

// ─── remove_if_arc_present ───────────────────────────────────────────────────

bool test_pool_remove_if_arc_present() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
    auto id3 = fp.add(make_pool_solution(6.0, {{2, 1.0L}}, {10, 30}));

    auto removed = fp.global_remove_if_arc_present(10);

    if (removed.size() != 2) {
        LOG_ERROR("test_pool_remove_if_arc_present: expected 2 removed, got ",
                  removed.size(), '\n');
        return false;
    }
    if (fp.size() != 1) {
        LOG_ERROR("test_pool_remove_if_arc_present: expected 1 remaining, got ",
                  fp.size(), '\n');
        return false;
    }
    if (!fp.get(id2) || fp.get(id1) || fp.get(id3)) {
        LOG_ERROR("test_pool_remove_if_arc_present: wrong column kept/removed\n");
        return false;
    }
    return true;
}

// ─── FilteredSolutionPool ────────────────────────────────────────────────────

// new_filter: creates a FilteredSolutionPool with row/arc constraints
bool test_pool_new_filter() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));

    // new_filter with no predicate: all entries visible
    auto fp_all = pool.new_filter();
    if (fp_all.size() != 2) {
        LOG_ERROR("test_pool_new_filter: no-filter pool should have 2 entries, got ",
                  fp_all.size(), '\n');
        return false;
    }

    // new_filter with arc constraint: only id2 (no arc 10)
    auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));
    if (fp2.size() != 1 || !fp2.get(id2) || fp2.get(id1)) {
        LOG_ERROR("test_pool_new_filter: wrong initial population\n");
        return false;
    }
    return true;
}

// Auto-propagation: fp.add() forwards to all other registered FilteredSolutionPools
bool test_pool_autopropagation_add() {
    SolutionPool pool;
    // fp1: no filter (all); fp2: forbidden arc 10
    auto fp1 = pool.new_filter();
    auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));

    // Add s1 (no arc 10) via fp1: both pools should receive it
    auto id1 = fp1.add(make_pool_solution(5.0, {{0, 1.0L}}, {20, 21}));
    if (!fp1.get(id1) || !fp2.get(id1)) {
        LOG_ERROR("test_pool_autopropagation_add: s1 (no arc 10) not propagated correctly\n");
        return false;
    }

    // Add s2 (has arc 10) via fp1: fp1 receives it, fp2 does not
    auto id2 = fp1.add(make_pool_solution(7.0, {{1, 1.0L}}, {10, 11}));
    if (!fp1.get(id2)) {
        LOG_ERROR("test_pool_autopropagation_add: s2 not in fp1\n");
        return false;
    }
    if (fp2.get(id2)) {
        LOG_ERROR("test_pool_autopropagation_add: s2 should not be in fp2 (arc 10 forbidden)\n");
        return false;
    }
    return true;
}

// Auto-propagation: global_remove_if() removes from all registered FilteredSolutionPools
bool test_pool_autopropagation_remove() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));

    auto fp1 = pool.new_filter();
    auto fp2 = pool.new_filter();

    if (fp1.size() != 2 || fp2.size() != 2) {
        LOG_ERROR("test_pool_autopropagation_remove: both pools should have 2 entries\n");
        return false;
    }

    // Remove id1 globally: both filtered pools should lose it
    fp.global_remove_if([&](SolutionPool::ColumnId cid, const Solution&, const ColumnActivity&) {
        return cid == id1;
    });

    if (fp1.get(id1) || fp2.get(id1)) {
        LOG_ERROR("test_pool_autopropagation_remove: id1 still in filtered pools after removal\n");
        return false;
    }
    if (!fp1.get(id2) || !fp2.get(id2)) {
        LOG_ERROR("test_pool_autopropagation_remove: id2 should remain in filtered pools\n");
        return false;
    }
    if (fp1.size() != 1 || fp2.size() != 1) {
        LOG_ERROR("test_pool_autopropagation_remove: expected size 1 in each filtered pool\n");
        return false;
    }
    return true;
}

// FilteredSolutionPool destructor unregisters: adding after fp goes out of scope is safe
bool test_filtered_pool_destructor_unregisters() {
    SolutionPool pool;
    {
        auto fp = pool.new_filter();
        fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
        // fp goes out of scope here
    }
    // If fp was not unregistered, the next add would access dangling pointer → UB.
    auto fp2 = pool.new_filter();
    fp2.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
    if (fp2.size() != 2) {
        LOG_ERROR("test_filtered_pool_destructor_unregisters: expected size 2, got ",
                  fp2.size(), '\n');
        return false;
    }
    return true;
}

// FilteredSolutionPool::add(): main pool always gets it; subpool only if filter passes.
// The caller FilteredSolutionPool handles its own filtered_ids_ directly.
bool test_filtered_pool_add() {
    SolutionPool pool;
    auto fp = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));  // forbid arc 10

    // s1: no arc 10 → accepted by filter
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {20, 21}));
    // s2: uses arc 10 → rejected by filter (but added to main pool)
    auto id2 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {10, 11}));

    if (fp.size() != 1) {
        LOG_ERROR("test_filtered_pool_add: expected 1 in subpool, got ", fp.size(), '\n');
        return false;
    }
    if (!fp.get(id1) || fp.get(id2)) {
        LOG_ERROR("test_filtered_pool_add: filter not applied on add\n");
        return false;
    }
    // Verify both are in the main pool via an unfiltered view
    auto fp_all = pool.new_filter();
    if (!fp_all.get(id1) || !fp_all.get(id2)) {
        LOG_ERROR("test_filtered_pool_add: main pool missing entries\n");
        return false;
    }
    return true;
}

// price() prices only the filtered subset
bool test_filtered_pool_price() {
    SolutionPool pool;
    auto fp_all = pool.new_filter();
    auto id1 = fp_all.add(make_pool_solution(5.0, {{0, 1.0L}}, {20, 21}));
    auto id2 = fp_all.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));

    // SubPool: forbidden arc 10 → only s1 is in scope
    auto fp = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));

    auto priced = fp.price({20.0});  // rc = 5 - 20 = -15 for both
    if (priced.size() != 1 || priced[0].id != id1) {
        LOG_ERROR("test_filtered_pool_price: expected only s1 priced, got ", priced.size(), '\n');
        return false;
    }
    // s1's activity updated; s2 (filtered out) untouched
    auto act1 = fp_all.get_activity(id1);
    auto act2 = fp_all.get_activity(id2);
    if (!act1 || act1->use_count != 1 || act1->age != 0) {
        LOG_ERROR("test_filtered_pool_price: s1 activity wrong\n");
        return false;
    }
    if (!act2 || act2->use_count != 0 || act2->age != 0) {
        LOG_ERROR("test_filtered_pool_price: s2 activity should be untouched\n");
        return false;
    }
    return true;
}

// remove_if_arc_present (local): subpool loses entry, main pool keeps it
bool test_filtered_pool_remove_arc_backtrack() {
    SolutionPool pool;
    auto fp_all = pool.new_filter();
    auto id1 = fp_all.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 20}));
    auto id2 = fp_all.add(make_pool_solution(7.0, {{1, 1.0L}}, {30, 40}));

    FilteredSolutionPool fp(pool);  // no base filter: both entries present
    if (fp.size() != 2) {
        LOG_ERROR("test_filtered_pool_remove_arc_backtrack: expected 2 initial, got ",
                  fp.size(), '\n');
        return false;
    }

    auto removed = fp.remove_if_arc_present(10);
    if (removed.size() != 1 || removed[0] != id1) {
        LOG_ERROR("test_filtered_pool_remove_arc_backtrack: wrong entry removed from subpool\n");
        return false;
    }
    if (fp.size() != 1 || fp.get(id1) || !fp.get(id2)) {
        LOG_ERROR("test_filtered_pool_remove_arc_backtrack: subpool state wrong after remove\n");
        return false;
    }

    // Main pool must NOT have lost s1
    if (!fp_all.get(id1)) {
        LOG_ERROR("test_filtered_pool_remove_arc_backtrack: main pool lost s1 (backtrack broken)\n");
        return false;
    }

    // Simulate backtrack: fp goes out of scope. Rebuild for parent node.
    {
        FilteredSolutionPool parent_fp(pool);
        if (parent_fp.size() != 2) {
            LOG_ERROR("test_filtered_pool_remove_arc_backtrack: after backtrack, expected 2, got ",
                      parent_fp.size(), '\n');
            return false;
        }
    }
    return true;
}

// Activity is shared: update via FilteredSolutionPool is visible through main pool and vice versa
bool test_filtered_pool_activity_shared() {
    SolutionPool pool;
    auto fp_all = pool.new_filter();
    auto id1 = fp_all.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp_all.add(make_pool_solution(7.0, {{0, 1.0L}}, {20, 21}));

    FilteredSolutionPool fp(pool);  // no filter — same entries

    // s1 rc = 5 - 6 = -1 → returned; s2 rc = 7 - 6 = 1 → NOT returned, age++
    (void)fp.price({6.0});

    auto act1 = fp_all.get_activity(id1);
    auto act2 = fp_all.get_activity(id2);
    if (!act1 || act1->use_count != 1 || act1->age != 0) {
        LOG_ERROR("test_filtered_pool_activity_shared: s1 activity via fp_all wrong\n");
        return false;
    }
    if (!act2 || act2->use_count != 0 || act2->age != 1) {
        LOG_ERROR("test_filtered_pool_activity_shared: s2 activity via fp_all wrong\n");
        return false;
    }

    // Update via fp_all; visible through fp
    fp_all.update_activity({id1, id2});  // both in basis
    auto fp_act2 = fp.get_activity(id2);
    if (!fp_act2 || fp_act2->use_count != 1 || fp_act2->age != 0) {
        LOG_ERROR("test_filtered_pool_activity_shared: shared activity update failed\n");
        return false;
    }
    return true;
}

// No-filter constructor
bool test_filtered_pool_no_filter() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
    fp.add(make_pool_solution(3.0, {{2, 1.0L}}, {30, 31}));

    FilteredSolutionPool fp2(pool);
    if (fp2.size() != 3 || fp2.size() != fp.size()) {
        LOG_ERROR("test_filtered_pool_no_filter: expected 3, got ", fp2.size(), '\n');
        return false;
    }
    return true;
}

// Chain filtering: fp.new_filter(pred) produces a further-narrowed view
bool test_filtered_pool_chain_filter() {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // s1: arc 10, row 0; s2: arc 20, row 0; s3: arc 10, row 1
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(6.0, {{0, 1.0L}}, {20, 21}));
    auto id3 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {10, 30}));

    // Base: compulsory row 0 → s1, s2
    auto fp1 = pool.new_filter(FilteredSolutionPool::make_filter({0}));
    if (fp1.size() != 2 || !fp1.get(id1) || !fp1.get(id2)) {
        LOG_ERROR("test_filtered_pool_chain_filter: fp1 should have s1 and s2\n");
        return false;
    }

    // Chain: also forbid arc 10 → only s2 remains
    auto fp2 = fp1.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));
    if (fp2.size() != 1 || !fp2.get(id2) || fp2.get(id1) || fp2.get(id3)) {
        LOG_ERROR("test_filtered_pool_chain_filter: fp2 should only have s2, got size=",
                  fp2.size(), '\n');
        return false;
    }

    // Adding a new solution: it propagates through the combined filter
    auto id4 = fp.add(make_pool_solution(4.0, {{0, 1.0L}}, {25, 26}));  // row 0, no arc 10/20
    if (!fp1.get(id4) || !fp2.get(id4)) {
        LOG_ERROR("test_filtered_pool_chain_filter: new column id4 should be in fp1 and fp2\n");
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

    run(test_pool_add_deduplication,               "test_pool_add_deduplication");
    run(test_pool_add_batch,                       "test_pool_add_batch");
    run(test_pool_get_by_id,                       "test_pool_get_by_id");
    run(test_pool_price_threshold,                 "test_pool_price_threshold");
    run(test_pool_price_does_not_mutate_stored_cost,
                                                   "test_pool_price_does_not_mutate_stored_cost");
    run(test_pool_price_out_of_range_dual,         "test_pool_price_out_of_range_dual");
    run(test_pool_activity_tracking,               "test_pool_activity_tracking");
    run(test_pool_price_compulsory_rows,           "test_pool_price_compulsory_rows");
    run(test_pool_price_forbidden_rows,            "test_pool_price_forbidden_rows");
    run(test_pool_price_combined_filters,          "test_pool_price_combined_filters");
    run(test_pool_external_variable_association,   "test_pool_external_variable_association");
    run(test_pool_remove_if_by_cost,               "test_pool_remove_if_by_cost");
    run(test_pool_remove_stale_by_age,             "test_pool_remove_stale_by_age");
    run(test_pool_remove_stale_by_usage_rate,      "test_pool_remove_stale_by_usage_rate");
    run(test_pool_remove_preserves_id_consistency, "test_pool_remove_preserves_id_consistency");
    run(test_pool_update_activity,                 "test_pool_update_activity");
    run(test_pool_price_arc_filters,               "test_pool_price_arc_filters");
    run(test_pool_remove_if_arc_present,           "test_pool_remove_if_arc_present");
    run(test_pool_new_filter,                      "test_pool_new_filter");
    run(test_pool_autopropagation_add,             "test_pool_autopropagation_add");
    run(test_pool_autopropagation_remove,          "test_pool_autopropagation_remove");
    run(test_filtered_pool_destructor_unregisters, "test_filtered_pool_destructor_unregisters");
    run(test_filtered_pool_add,                    "test_filtered_pool_add");
    run(test_filtered_pool_price,                  "test_filtered_pool_price");
    run(test_filtered_pool_remove_arc_backtrack,   "test_filtered_pool_remove_arc_backtrack");
    run(test_filtered_pool_activity_shared,        "test_filtered_pool_activity_shared");
    run(test_filtered_pool_no_filter,              "test_filtered_pool_no_filter");
    run(test_filtered_pool_chain_filter,           "test_filtered_pool_chain_filter");

    return {passed, total};
}