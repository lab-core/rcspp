// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <initializer_list>
#include <list>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
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

    std::vector<size_t> arc_list(arc_ids);
    std::vector<size_t> node_list;
    for (size_t i = 0; i <= arc_ids.size(); ++i) {
        node_list.push_back(i);
    }
    return Solution(col_cost, std::move(node_list), std::move(arc_list), std::move(col));
}

}  // namespace

// ─── Solution hashing (H-1) ───────────────────────────────────────────────────

// A default-constructed Solution must carry the empty-path hash (FNV offset basis), matching a
// value-constructed empty-path Solution — so operator== / std::hash are consistent.
TEST(Solution, DefaultConstructedHashMatchesEmptyPath) {
    Solution def;                 // default ctor now computes the (empty-path) hash
    Solution param(0.0, {}, {});  // value ctor, empty path
    EXPECT_EQ(def.get_hash(), FNV_OFFSET_BASIS);
    EXPECT_EQ(def.get_hash(), param.get_hash());
    EXPECT_TRUE(def == param);  // was false before (0 vs FNV_OFFSET_BASIS) despite equal paths
}

// rehash() recomputes the hash after path_arc_ids is mutated directly, matching the value ctor.
TEST(Solution, RehashReflectsPathMutation) {
    Solution s;  // empty path
    const auto empty_hash = s.get_hash();
    s.path_arc_ids = {10, 11};  // mutate the public member directly
    s.rehash();                 // recompute (the Python path_arc_ids setter does this for you)
    EXPECT_NE(s.get_hash(), empty_hash);
    Solution ref(0.0, {}, {10, 11});  // value ctor with the same path
    EXPECT_EQ(s.get_hash(), ref.get_hash());
    EXPECT_TRUE(s == ref);
}

// Distinct arc paths hash differently (the index actually discriminates).
TEST(Solution, DistinctPathsDistinctHash) {
    Solution a(0.0, {}, {10, 11});
    Solution b(0.0, {}, {20, 21});
    EXPECT_NE(a.get_hash(), b.get_hash());
}

// ─── add / deduplication ────────────────────────────────────────────────────

TEST(SolutionPool, AddDeduplication) {
    SolutionPool pool;
    auto fp = pool.new_filter();

    auto s1 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});
    auto s2 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});  // identical to s1
    auto s3 = make_pool_solution(7.0, {{1, 1.0L}}, {20, 21});  // distinct

    auto id1 = fp.add(s1);
    auto id2 = fp.add(s2);  // must return same id (exact duplicate)
    auto id3 = fp.add(s3);

    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, id3);
    EXPECT_NE(id1, SolutionPool::kNoId);
    EXPECT_NE(id3, SolutionPool::kNoId);
    EXPECT_EQ(fp.size(), 2u);
}

TEST(SolutionPool, AddBatch) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto s1 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});
    auto s2 = make_pool_solution(7.0, {{1, 1.0L}}, {20, 21});

    auto ids = fp.add({s1, s2, s1});  // s1 added twice: second must return same id

    EXPECT_EQ(fp.size(), 2u);
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], ids[2]);
}

// ─── duplicate add refreshes the stored column (L-3) ──────────────────────────

TEST(SolutionPool, DuplicateAddRefreshesColumnAndResetsAge) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    const auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));

    // Age the entry with a pricing round that does not return it (rc = 5 - 0 > 0 → age++).
    (void)fp.price({0.0});
    {
        auto act = fp.get_activity(id1);
        ASSERT_TRUE(act.has_value());
        EXPECT_EQ(act->age, 1u);
    }

    // Re-propose the SAME arc path with a different (cheaper) column.
    const auto id2 = fp.add(make_pool_solution(3.0, {{1, 2.0L}}, {10, 11}));
    EXPECT_EQ(id1, id2);       // deduped to the same column id
    EXPECT_EQ(fp.size(), 1u);  // no new entry created

    // The stored column reflects the LATEST cost/rows, not the first-seen values.
    auto got = fp.get(id1);
    ASSERT_TRUE(got.has_value());
    EXPECT_NEAR(got->column.cost, 3.0, 1e-9);
    ASSERT_EQ(got->column.rows.size(), 1u);
    EXPECT_EQ(got->column.rows[0].index, 1u);
    EXPECT_NEAR(static_cast<double>(got->column.rows[0].coefficient), 2.0, 1e-9);

    // Re-proposing resets age (a regenerated column is fresh, not stale).
    auto act = fp.get_activity(id1);
    ASSERT_TRUE(act.has_value());
    EXPECT_EQ(act->age, 0u);
}

// ─── duplicate add keeps price() consistent with get() (L-3b) ────────────────
// Regression for: refresh updated solution.column but left the SoA LP store
// (used by price_subset_locked) pointing at the old cost/coefs.

TEST(SolutionPool, DuplicateAddRefreshesSoaForPricing) {
    SolutionPool pool;
    auto fp = pool.new_filter();

    // Original column: cost=10, row 0 coef=1.
    // With duals=[0], rc = 10-0 = 10 (above threshold 0, not returned).
    const auto id1 = fp.add(make_pool_solution(10.0, {{0, 1.0L}}, {10, 11}));
    {
        auto priced = fp.price({0.0}, 0.0);
        EXPECT_EQ(priced.size(), 0u);  // rc=10 > 0
    }

    // Re-add the same path with a cheaper column: cost=2, row 0 coef=1.
    // rc = 2 - 3*1 = -1 (below 0, should be returned).
    const auto id2 = fp.add(make_pool_solution(2.0, {{0, 1.0L}}, {10, 11}));
    EXPECT_EQ(id1, id2);  // same id (deduped)

    // price() must use the REFRESHED LP data.
    auto priced = fp.price({3.0}, 0.0);
    ASSERT_EQ(priced.size(), 1u);
    EXPECT_EQ(priced[0].id, id1);
    EXPECT_NEAR(priced[0].reduced_cost, -1.0, 1e-9);  // 2 - 3*1 = -1
}

// ─── get ────────────────────────────────────────────────────────────────────

TEST(SolutionPool, GetById) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto s1 = make_pool_solution(5.0, {{0, 1.0L}}, {10, 11});
    auto id = fp.add(s1);

    auto retrieved = fp.get(id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_NEAR(retrieved->column.cost, 5.0, 1e-9);

    auto missing = fp.get(SolutionPool::kNoId);
    EXPECT_FALSE(missing.has_value());
}

// ─── price ──────────────────────────────────────────────────────────────────

TEST(SolutionPool, PriceThreshold) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // column.cost=10, rows=[{0,1}]
    // duals=[3]:  rc = 10 - 3 = 7   → positive, not returned
    // duals=[11]: rc = 10 - 11 = -1 → negative, returned
    auto id = fp.add(make_pool_solution(10.0, {{0, 1.0L}}, {0, 1}));

    auto r1 = fp.price({3.0});
    EXPECT_TRUE(r1.empty());

    auto r2 = fp.price({11.0});
    ASSERT_EQ(r2.size(), 1u);
    EXPECT_EQ(r2[0].id, id);
    EXPECT_NEAR(r2[0].reduced_cost, -1.0, 1e-9);
    // solution.cost must NOT be overwritten — still holds original arc cost
    EXPECT_NEAR(r2[0].solution->column.cost, 10.0, 1e-9);
}

// L-7: pricing accumulates the reduced cost in long double and narrows once — verify a fractional
// computation comes out correct (exercises the non-integer coefficient/dual path).
TEST(SolutionPool, PriceFractionalCoefficients) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // column.cost=2.5, rows=[{0,0.1},{1,0.2}], duals=[3,4]: rc = 2.5 - 0.1*3 - 0.2*4 = 1.4
    auto id = fp.add(make_pool_solution(2.5, {{0, 0.1L}, {1, 0.2L}}, {10, 11}));
    auto r = fp.price({3.0, 4.0}, /*threshold=*/2.0);  // rc 1.4 < 2.0 → returned
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].id, id);
    EXPECT_NEAR(r[0].reduced_cost, 1.4, 1e-9);
}

TEST(SolutionPool, PriceDoesNotMutateStoredCost) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // column.cost=6, rows=[{0,2},{1,1}]
    // duals=[1,2]: rc = 6 - 2*1 - 1*2 = 2
    fp.add(make_pool_solution(6.0, {{0, 2.0L}, {1, 1.0L}}, {0, 1}));

    (void)fp.price({1.0, 2.0});

    // The stored column.cost must remain 6.0 (not 2.0)
    auto all = fp.get_all();
    EXPECT_NEAR(std::get<1>(all[0]).column.cost, 6.0, 1e-9);
}

TEST(SolutionPool, PriceOutOfRangeDual) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // rows=[{5, 1.0}] but only 3 duals provided → dual at index 5 treated as 0
    // rc = 8.0 - 0 = 8.0
    fp.add(make_pool_solution(8.0, {{5, 1.0L}}, {0, 1}));

    auto r = fp.price({1.0, 2.0, 3.0});  // only indices 0-2 provided
    EXPECT_TRUE(r.empty());

    // Verify stored cost unchanged
    auto all = fp.get_all();
    EXPECT_NEAR(std::get<1>(all[0]).column.cost, 8.0, 1e-9);
}

// ─── pool-managed activity tracking ─────────────────────────────────────────

TEST(SolutionPool, ActivityTracking) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // column.cost=10, rows=[{0,1}]
    // duals=[3]:  rc=7  (positive, age++)
    // duals=[11]: rc=-1 (negative, age=0, use_count++)
    auto id = fp.add(make_pool_solution(10.0, {{0, 1.0L}}, {0, 1}));

    EXPECT_EQ(fp.pricing_count(), 0u);

    {
        auto act = fp.get_activity(id);
        ASSERT_TRUE(act.has_value());
        EXPECT_EQ(act->created_at, 0u);
    }

    (void)fp.price({3.0});
    {
        auto act = fp.get_activity(id);
        ASSERT_TRUE(act.has_value());
        EXPECT_EQ(act->age, 1u);
        EXPECT_EQ(act->use_count, 0u);
        EXPECT_FALSE(act->last_was_negative);
        EXPECT_EQ(fp.pricing_count(), 1u);
    }

    (void)fp.price({11.0});
    {
        auto act = fp.get_activity(id);
        ASSERT_TRUE(act.has_value());
        EXPECT_EQ(act->age, 0u);
        EXPECT_EQ(act->use_count, 1u);
        EXPECT_TRUE(act->last_was_negative);
        EXPECT_NEAR(act->usage_rate(), 0.5, 1e-9);
    }

    (void)fp.price({3.0});
    (void)fp.price({3.0});
    {
        auto act = fp.get_activity(id);
        ASSERT_TRUE(act.has_value());
        EXPECT_EQ(act->age, 2u);
        EXPECT_EQ(act->use_count, 1u);
        EXPECT_NEAR(act->usage_rate(), 0.25, 1e-9);
    }
}

// ─── row filters via FilteredSolutionPool ────────────────────────────────────

TEST(SolutionPool, PriceCompulsoryRows) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}, {1, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(3.0, {{2, 1.0L}}, {20, 21}));

    std::vector<double> duals = {10.0, 10.0, 10.0};

    // compulsory_rows={0}: s1 has row 0; s2 does not
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({0}));
        auto r = fp2.price(duals);
        ASSERT_EQ(r.size(), 1u);
        EXPECT_EQ(r[0].id, id1);
    }
    // compulsory_rows={0,1}: only s1 has both
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({0, 1}));
        auto r = fp2.price(duals);
        ASSERT_EQ(r.size(), 1u);
        EXPECT_EQ(r[0].id, id1);
    }
    // compulsory_rows={0,2}: no column has both → empty
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({0, 2}));
        auto r = fp2.price(duals);
        EXPECT_TRUE(r.empty());
    }
    (void)id2;
}

TEST(SolutionPool, PriceForbiddenRows) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}, {1, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(3.0, {{2, 1.0L}}, {20, 21}));

    std::vector<double> duals = {10.0, 10.0, 10.0};

    // forbidden_rows={1}: s1 has row 1 → filtered; s2 does not → returned
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {1}));
        auto r = fp2.price(duals);
        ASSERT_EQ(r.size(), 1u);
        EXPECT_EQ(r[0].id, id2);
    }
    // forbidden_rows={0,2}: s1 (row 0) and s2 (row 2) both filtered → empty
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {0, 2}));
        auto r = fp2.price(duals);
        EXPECT_TRUE(r.empty());
    }
    (void)id1;
}

TEST(SolutionPool, PriceCombinedFilters) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    fp.add(make_pool_solution(5.0, {{0, 1.0L}, {1, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(3.0, {{0, 1.0L}}, {20, 21}));
    fp.add(make_pool_solution(4.0, {{1, 1.0L}, {2, 1.0L}}, {30, 31}));

    std::vector<double> duals = {10.0, 10.0, 10.0};

    // compulsory={0}, forbidden={1}: must have row 0 AND NOT row 1 → only s2
    auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({0}, {1}));
    auto r = fp2.price(duals);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].id, id2);
}

// ─── external variable association ──────────────────────────────────────────

TEST(SolutionPool, ExternalVariableAssociation) {
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
        EXPECT_TRUE(master1_vars.contains(pc.id));
    }

    for (const auto& pc : priced) {
        if (!master2_vars.contains(pc.id)) {
            master2_vars[pc.id] = 300;
        }
    }
    ASSERT_TRUE(master2_vars.contains(id2));
    EXPECT_EQ(master2_vars[id2], 300);
}

// ─── remove_if ───────────────────────────────────────────────────────────────

TEST(SolutionPool, RemoveIfByCost) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    fp.add(make_pool_solution(15.0, {{1, 1.0L}}, {20, 21}));
    fp.add(make_pool_solution(8.0, {{2, 1.0L}}, {30, 31}));

    auto removed_ids =
        fp.global_remove_if([](SolutionPool::ColumnId, const Solution& sol, const ColumnActivity&) {
            return sol.column.cost > 10.0;
        });

    EXPECT_EQ(removed_ids.size(), 1u);
    EXPECT_EQ(fp.size(), 2u);
    for (const auto& [col_id, sol, act] : fp.get_all()) {
        EXPECT_LE(sol.column.cost, 10.0);
    }
}

// ─── remove_stale ────────────────────────────────────────────────────────────

TEST(SolutionPool, RemoveStaleByAge) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(8.0, {{0, 1.0L}}, {20, 21}));

    (void)fp.price({6.0});  // s1: age=0, s2: age=1
    (void)fp.price({6.0});  // s1: age=0, s2: age=2

    auto removed_ids = fp.global_remove_stale(1);

    ASSERT_EQ(removed_ids.size(), 1u);
    EXPECT_EQ(removed_ids[0], id2);
    EXPECT_EQ(fp.size(), 1u);

    auto all = fp.get_all();
    EXPECT_NEAR(std::get<1>(all[0]).column.cost, 5.0, 1e-9);
    (void)id1;
}

TEST(SolutionPool, RemoveStaleByUsageRate) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(3.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(100.0, {{1, 1.0L}}, {20, 21}));

    (void)fp.price({5.0, 0.0});
    (void)fp.price({5.0, 0.0});
    (void)fp.price({5.0, 0.0});
    (void)fp.price({5.0, 0.0});

    // s1: use_count=4, usage_rate=1.0 → kept; s2: use_count=0, usage_rate=0.0 < 0.1 → removed
    auto removed_ids = fp.global_remove_stale(/*max_age=*/100, /*min_usage_rate=*/0.1);

    ASSERT_EQ(removed_ids.size(), 1u);
    EXPECT_EQ(removed_ids[0], id2);
    EXPECT_EQ(fp.size(), 1u);
    EXPECT_TRUE(fp.get(id1).has_value());
}

TEST(SolutionPool, RemovePreservesIdConsistency) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(8.0, {{1, 1.0L}}, {20, 21}));
    auto id3 = fp.add(make_pool_solution(12.0, {{2, 1.0L}}, {30, 31}));

    fp.global_remove_if([&](SolutionPool::ColumnId cid, const Solution&, const ColumnActivity&) {
        return cid == id2;
    });

    EXPECT_EQ(fp.size(), 2u);
    EXPECT_TRUE(fp.get(id1).has_value());
    EXPECT_TRUE(fp.get(id3).has_value());
    EXPECT_FALSE(fp.get(id2).has_value());

    auto new_id = fp.add(make_pool_solution(8.0, {{1, 1.0L}}, {20, 21}));
    EXPECT_EQ(fp.size(), 3u);
    EXPECT_NE(new_id, id2);
}

// ─── update_activity (LP basis membership) ───────────────────────────────────

TEST(SolutionPool, UpdateActivity) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(8.0, {{1, 1.0L}}, {20, 21}));

    fp.update_activity({id1});  // id1 in basis, id2 not

    {
        auto act1 = fp.get_activity(id1);
        auto act2 = fp.get_activity(id2);
        ASSERT_TRUE(act1.has_value());
        ASSERT_TRUE(act2.has_value());
        EXPECT_EQ(act1->age, 0u);
        EXPECT_EQ(act1->use_count, 0u);  // basis membership does not bump use_count
        EXPECT_TRUE(act1->last_was_negative);
        EXPECT_EQ(act2->age, 1u);
        EXPECT_EQ(act2->use_count, 0u);
        EXPECT_FALSE(act2->last_was_negative);
    }

    fp.update_activity({});
    {
        auto act1 = fp.get_activity(id1);
        auto act2 = fp.get_activity(id2);
        ASSERT_TRUE(act1.has_value());
        ASSERT_TRUE(act2.has_value());
        EXPECT_EQ(act1->age, 1u);
        EXPECT_EQ(act1->use_count, 0u);  // basis membership does not bump use_count
        EXPECT_EQ(act2->age, 2u);
        EXPECT_EQ(act2->use_count, 0u);
    }
    EXPECT_EQ(fp.pricing_count(), 0u);
}

// ─── usage_rate semantics (L-1 / L-2 / L-4) ──────────────────────────────────

// L-1: basis membership (update_activity) must not inflate use_count or push usage_rate above 1.
TEST(SolutionPool, UsageRateIsAFractionNotInflatedByBasis) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id = fp.add(make_pool_solution(1.0, {{0, 1.0L}}, {10, 11}));
    (void)fp.price({2.0});  // rc = 1 - 2 = -1 < 0 → returned: priced_count=1, use_count=1
    for (int i = 0; i < 5; ++i) {
        fp.update_activity({id});  // in basis 5×
    }
    auto act = fp.get_activity(id);
    ASSERT_TRUE(act.has_value());
    EXPECT_EQ(act->priced_count, 1u);
    EXPECT_EQ(act->use_count, 1u);              // basis does not bump use_count
    EXPECT_NEAR(act->usage_rate(), 1.0, 1e-9);  // 1/1, never > 1
}

// L-4: usage_rate depends only on the column's own pricings, not on other views' pricing traffic.
TEST(SolutionPool, UsageRateIsViewTrafficIndependent) {
    SolutionPool pool;
    auto root = pool.new_filter();
    root.add(make_pool_solution(1.0, {{0, 1.0L}}, {10, 11}));              // column A (arc 10)
    auto id_b = root.add(make_pool_solution(1.0, {{0, 1.0L}}, {20, 21}));  // column B (arc 20)

    auto fp_a = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {10}));  // only A
    auto fp_b = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {20}));  // only B

    for (int i = 0; i < 10; ++i) {
        (void)fp_a.price({2.0});  // high-traffic view: A priced 10× (rc = -1 → returned)
    }
    for (int i = 0; i < 2; ++i) {
        (void)fp_b.price({2.0});  // low-traffic view: B priced 2×
    }

    auto act_b = fp_b.get_activity(id_b);
    ASSERT_TRUE(act_b.has_value());
    EXPECT_EQ(act_b->priced_count, 2u);  // only B's own pricings, not A's 10 rounds
    EXPECT_EQ(act_b->use_count, 2u);
    EXPECT_NEAR(act_b->usage_rate(),
                1.0,
                1e-9);  // 2/2 = 1.0 (would be 2/12 under the pool-wide bug)
}

// L-2: a never-priced column must not be evicted by the usage-rate criterion.
TEST(SolutionPool, RemoveStaleKeepsNeverPricedColumn) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(10.0, {{0, 1.0L}}, {10, 11}));
    (void)fp.price({0.0});  // rc = 10 > 0 → priced but not returned (priced_count=1, use_count=0)
    (void)fp.price({0.0});
    // Added after pricing ⇒ never priced (priced_count == 0).
    auto id2 = fp.add(make_pool_solution(5.0, {{1, 1.0L}}, {20, 21}));

    auto removed = fp.global_remove_stale(/*max_age=*/1000, /*min_usage_rate=*/0.5);
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0], id1);            // priced & unused → evicted
    EXPECT_TRUE(fp.get(id2).has_value());  // never-priced column survives
    EXPECT_FALSE(fp.get(id1).has_value());
    EXPECT_EQ(fp.size(), 1u);
}

// ─── arc-based filters via FilteredSolutionPool ──────────────────────────────

TEST(SolutionPool, PriceArcFilters) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}, {1, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(3.0, {{0, 1.0L}, {1, 1.0L}}, {20, 21}));

    std::vector<double> duals = {10.0, 10.0};

    // compulsory_arc_ids={10}: only s1 has arc 10 in path
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {10}));
        auto r = fp2.price(duals);
        ASSERT_EQ(r.size(), 1u);
        EXPECT_EQ(r[0].id, id1);
    }

    // forbidden_arc_ids={10}: s1 filtered; only s2 returned
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));
        auto r = fp2.price(duals);
        ASSERT_EQ(r.size(), 1u);
        EXPECT_EQ(r[0].id, id2);
    }

    // forbidden_arc_ids={10,20}: both paths filtered → empty
    {
        auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10, 20}));
        auto r = fp2.price(duals);
        EXPECT_TRUE(r.empty());
    }
}

// ─── remove_if_arc_present ───────────────────────────────────────────────────

TEST(SolutionPool, RemoveIfArcPresent) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
    auto id3 = fp.add(make_pool_solution(6.0, {{2, 1.0L}}, {10, 30}));

    auto removed = fp.global_remove_if_arc_present(10);

    EXPECT_EQ(removed.size(), 2u);
    EXPECT_EQ(fp.size(), 1u);
    EXPECT_TRUE(fp.get(id2).has_value());
    EXPECT_FALSE(fp.get(id1).has_value());
    EXPECT_FALSE(fp.get(id3).has_value());
}

// ─── FilteredSolutionPool ────────────────────────────────────────────────────

TEST(SolutionPool, NewFilter) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));

    // new_filter with no predicate: all entries visible
    auto fp_all = pool.new_filter();
    EXPECT_EQ(fp_all.size(), 2u);

    // new_filter with arc constraint: only id2 (no arc 10)
    auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));
    EXPECT_EQ(fp2.size(), 1u);
    EXPECT_TRUE(fp2.get(id2).has_value());
    EXPECT_FALSE(fp2.get(id1).has_value());
}

// Auto-propagation: fp.add() forwards to all other registered FilteredSolutionPools
TEST(SolutionPool, AutopropagationAdd) {
    SolutionPool pool;
    // fp1: no filter (all); fp2: forbidden arc 10
    auto fp1 = pool.new_filter();
    auto fp2 = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));

    // Add s1 (no arc 10) via fp1: both pools should receive it
    auto id1 = fp1.add(make_pool_solution(5.0, {{0, 1.0L}}, {20, 21}));
    EXPECT_TRUE(fp1.get(id1).has_value());
    EXPECT_TRUE(fp2.get(id1).has_value());

    // Add s2 (has arc 10) via fp1: fp1 receives it, fp2 does not
    auto id2 = fp1.add(make_pool_solution(7.0, {{1, 1.0L}}, {10, 11}));
    EXPECT_TRUE(fp1.get(id2).has_value());
    EXPECT_FALSE(fp2.get(id2).has_value());
}

// Auto-propagation: global_remove_if() removes from all registered FilteredSolutionPools
TEST(SolutionPool, AutopropagationRemove) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));

    auto fp1 = pool.new_filter();
    auto fp2 = pool.new_filter();

    EXPECT_EQ(fp1.size(), 2u);
    EXPECT_EQ(fp2.size(), 2u);

    // Remove id1 globally: both filtered pools should lose it
    fp.global_remove_if([&](SolutionPool::ColumnId cid, const Solution&, const ColumnActivity&) {
        return cid == id1;
    });

    EXPECT_FALSE(fp1.get(id1).has_value());
    EXPECT_FALSE(fp2.get(id1).has_value());
    EXPECT_TRUE(fp1.get(id2).has_value());
    EXPECT_TRUE(fp2.get(id2).has_value());
    EXPECT_EQ(fp1.size(), 1u);
    EXPECT_EQ(fp2.size(), 1u);
}

// FilteredSolutionPool destructor unregisters: adding after fp goes out of scope is safe
TEST(FilteredSolutionPool, DestructorUnregisters) {
    SolutionPool pool;
    {
        auto fp = pool.new_filter();
        fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
        // fp goes out of scope here
    }
    // If fp was not unregistered, the next add would access dangling pointer → UB.
    auto fp2 = pool.new_filter();
    fp2.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
    EXPECT_EQ(fp2.size(), 2u);
}

// FilteredSolutionPool::add(): main pool always gets it; subpool only if filter passes.
TEST(FilteredSolutionPool, Add) {
    SolutionPool pool;
    auto fp =
        pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));  // forbid arc 10

    // s1: no arc 10 → accepted by filter
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {20, 21}));
    // s2: uses arc 10 → rejected by filter (but added to main pool)
    auto id2 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {10, 11}));

    EXPECT_EQ(fp.size(), 1u);
    EXPECT_TRUE(fp.get(id1).has_value());
    EXPECT_FALSE(fp.get(id2).has_value());

    // Verify both are in the main pool via an unfiltered view
    auto fp_all = pool.new_filter();
    EXPECT_TRUE(fp_all.get(id1).has_value());
    EXPECT_TRUE(fp_all.get(id2).has_value());
}

// price() prices only the filtered subset
TEST(FilteredSolutionPool, Price) {
    SolutionPool pool;
    auto fp_all = pool.new_filter();
    auto id1 = fp_all.add(make_pool_solution(5.0, {{0, 1.0L}}, {20, 21}));
    auto id2 = fp_all.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));

    // SubPool: forbidden arc 10 → only s1 is in scope
    auto fp = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));

    auto priced = fp.price({20.0});  // rc = 5 - 20 = -15 for both
    ASSERT_EQ(priced.size(), 1u);
    EXPECT_EQ(priced[0].id, id1);

    // s1's activity updated; s2 (filtered out) untouched
    auto act1 = fp_all.get_activity(id1);
    auto act2 = fp_all.get_activity(id2);
    ASSERT_TRUE(act1.has_value());
    ASSERT_TRUE(act2.has_value());
    EXPECT_EQ(act1->use_count, 1u);
    EXPECT_EQ(act1->age, 0u);
    EXPECT_EQ(act2->use_count, 0u);
    EXPECT_EQ(act2->age, 0u);
}

// remove_if_arc_present (local): subpool loses entry, main pool keeps it
TEST(FilteredSolutionPool, RemoveArcBacktrack) {
    SolutionPool pool;
    auto fp_all = pool.new_filter();
    auto id1 = fp_all.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 20}));
    auto id2 = fp_all.add(make_pool_solution(7.0, {{1, 1.0L}}, {30, 40}));

    FilteredSolutionPool fp(pool);  // no base filter: both entries present
    EXPECT_EQ(fp.size(), 2u);

    auto removed = fp.remove_if_arc_present(10);
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0], id1);
    EXPECT_EQ(fp.size(), 1u);
    EXPECT_FALSE(fp.get(id1).has_value());
    EXPECT_TRUE(fp.get(id2).has_value());

    // Main pool must NOT have lost s1
    EXPECT_TRUE(fp_all.get(id1).has_value());

    // Simulate backtrack: fp goes out of scope. Rebuild for parent node.
    {
        FilteredSolutionPool parent_fp(pool);
        EXPECT_EQ(parent_fp.size(), 2u);
    }
}

// Activity is shared: update via FilteredSolutionPool is visible through main pool and vice versa
TEST(FilteredSolutionPool, ActivityShared) {
    SolutionPool pool;
    auto fp_all = pool.new_filter();
    auto id1 = fp_all.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp_all.add(make_pool_solution(7.0, {{0, 1.0L}}, {20, 21}));

    FilteredSolutionPool fp(pool);  // no filter — same entries

    // s1 rc = 5 - 6 = -1 → returned; s2 rc = 7 - 6 = 1 → NOT returned, age++
    (void)fp.price({6.0});

    auto act1 = fp_all.get_activity(id1);
    auto act2 = fp_all.get_activity(id2);
    ASSERT_TRUE(act1.has_value());
    ASSERT_TRUE(act2.has_value());
    EXPECT_EQ(act1->use_count, 1u);
    EXPECT_EQ(act1->age, 0u);
    EXPECT_EQ(act2->use_count, 0u);
    EXPECT_EQ(act2->age, 1u);

    // Update via fp_all; visible through fp
    fp_all.update_activity({id1, id2});  // both in basis
    auto fp_act2 = fp.get_activity(id2);
    ASSERT_TRUE(fp_act2.has_value());
    EXPECT_EQ(fp_act2->use_count, 0u);  // basis membership does not bump use_count
    EXPECT_EQ(fp_act2->age, 0u);        // but it resets age — and that update is visible via fp
}

// No-filter constructor
TEST(FilteredSolutionPool, NoFilter) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
    fp.add(make_pool_solution(3.0, {{2, 1.0L}}, {30, 31}));

    FilteredSolutionPool fp2(pool);
    EXPECT_EQ(fp2.size(), 3u);
    EXPECT_EQ(fp2.size(), fp.size());
}

// Chain filtering: fp.new_filter(pred) produces a further-narrowed view
TEST(FilteredSolutionPool, ChainFilter) {
    SolutionPool pool;
    auto fp = pool.new_filter();
    // s1: arc 10, row 0; s2: arc 20, row 0; s3: arc 10, row 1
    auto id1 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
    auto id2 = fp.add(make_pool_solution(6.0, {{0, 1.0L}}, {20, 21}));
    auto id3 = fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {10, 30}));

    // Base: compulsory row 0 → s1, s2
    auto fp1 = pool.new_filter(FilteredSolutionPool::make_filter({0}));
    EXPECT_EQ(fp1.size(), 2u);
    EXPECT_TRUE(fp1.get(id1).has_value());
    EXPECT_TRUE(fp1.get(id2).has_value());

    // Chain: also forbid arc 10 → only s2 remains
    auto fp2 = fp1.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));
    EXPECT_EQ(fp2.size(), 1u);
    EXPECT_TRUE(fp2.get(id2).has_value());
    EXPECT_FALSE(fp2.get(id1).has_value());
    EXPECT_FALSE(fp2.get(id3).has_value());

    // Adding a new solution: it propagates through the combined filter
    auto id4 = fp.add(make_pool_solution(4.0, {{0, 1.0L}}, {25, 26}));  // row 0, no arc 10/20
    EXPECT_TRUE(fp1.get(id4).has_value());
    EXPECT_TRUE(fp2.get(id4).has_value());
}

// ─── concurrency / locking ────────────────────────────────────────────────────
//
// These verify the pool's locking discipline. The "no-deadlock" tests pass a predicate or filter
// that re-enters the pool: because predicates and filters are evaluated with no lock held, they
// must complete; were they run under the (non-recursive) pool mutex they would self-deadlock. A
// watchdog turns a genuine deadlock into a failed assertion instead of hanging the test binary.
// The stress tests check that concurrent add/price/remove/read keep the per-view containers
// consistent; run them under a thread sanitizer (-fsanitize=thread) to also catch data races.

namespace {

// Runs `body` (which returns its own pass/fail) on a detached worker and returns
// {completed_within_timeout, body_result}. The shared state is held in shared_ptrs captured by
// the worker, so a timed-out (deadlocked) worker is safely leaked rather than left dangling.
template <typename Body>
std::pair<bool, bool> run_guarded(std::chrono::milliseconds timeout, Body body) {
    auto result = std::make_shared<std::atomic<bool>>(false);
    auto prom = std::make_shared<std::promise<void>>();
    std::future<void> fut = prom->get_future();
    std::thread([prom, result, body = std::move(body)]() mutable {
        const bool ok = body();
        result->store(ok);
        prom->set_value();
    }).detach();
    const bool completed = fut.wait_for(timeout) == std::future_status::ready;
    return {completed, result->load()};
}

constexpr std::chrono::seconds kWatchdog{5};

}  // namespace

// A global_remove_if predicate that reads the pool must not deadlock (it runs off-lock).
TEST(SolutionPoolConcurrency, GlobalRemoveIfReentrantPredicateDoesNotDeadlock) {
    auto [completed, passed] = run_guarded(kWatchdog, [] {
        SolutionPool pool;
        auto fp = pool.new_filter();
        const auto id0 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
        fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
        const auto removed = fp.global_remove_if(
            [&fp, id0](SolutionPool::ColumnId cid, const Solution&, const ColumnActivity&) {
                (void)fp.size();  // re-entrant reads from inside the predicate
                (void)fp.get(cid);
                (void)fp.get_all();
                return cid == id0;
            });
        return removed.size() == 1 && fp.size() == 1;
    });
    ASSERT_TRUE(completed) << "global_remove_if deadlocked on a re-entrant predicate";
    EXPECT_TRUE(passed);
}

// Same for the local remove_if.
TEST(SolutionPoolConcurrency, LocalRemoveIfReentrantPredicateDoesNotDeadlock) {
    auto [completed, passed] = run_guarded(kWatchdog, [] {
        SolutionPool pool;
        auto fp = pool.new_filter();
        const auto id0 = fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
        fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
        const auto removed = fp.remove_if(
            [&fp, id0](SolutionPool::ColumnId cid, const Solution&, const ColumnActivity&) {
                (void)fp.size();
                (void)fp.get_all();
                return cid == id0;
            });
        return removed.size() == 1 && fp.size() == 1;
    });
    ASSERT_TRUE(completed) << "remove_if deadlocked on a re-entrant predicate";
    EXPECT_TRUE(passed);
}

// A filter that reads the pool while a view is being constructed must not deadlock.
TEST(SolutionPoolConcurrency, NewFilterReentrantFilterDoesNotDeadlock) {
    auto [completed, passed] = run_guarded(kWatchdog, [] {
        SolutionPool pool;
        auto seed = pool.new_filter();
        seed.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
        seed.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
        auto fp = pool.new_filter([&seed](const Solution&) {
            (void)seed.size();  // re-entrant read during construction
            return true;
        });
        return fp.size() == 2;
    });
    ASSERT_TRUE(completed) << "new_filter deadlocked on a re-entrant filter";
    EXPECT_TRUE(passed);
}

// Same for add_filter (in-place narrowing).
TEST(SolutionPoolConcurrency, AddFilterReentrantFilterDoesNotDeadlock) {
    auto [completed, passed] = run_guarded(kWatchdog, [] {
        SolutionPool pool;
        auto fp = pool.new_filter();
        fp.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
        fp.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
        fp.add_filter([&fp](const Solution&) {
            (void)fp.size();  // re-entrant read while pruning
            return true;
        });
        return fp.size() == 2;
    });
    ASSERT_TRUE(completed) << "add_filter deadlocked on a re-entrant filter";
    EXPECT_TRUE(passed);
}

// A filter that throws during construction must rethrow AND deregister the half-built view, so the
// pool's registration list is never left with a dangling pointer.
TEST(SolutionPoolConcurrency, ThrowingFilterDuringConstructionLeavesPoolUsable) {
    SolutionPool pool;
    {
        auto seed = pool.new_filter();
        seed.add(make_pool_solution(5.0, {{0, 1.0L}}, {10, 11}));
        seed.add(make_pool_solution(7.0, {{1, 1.0L}}, {20, 21}));
    }
    EXPECT_THROW(
        {
            auto bad =
                pool.new_filter([](const Solution&) -> bool { throw std::runtime_error("boom"); });
            (void)bad.size();
        },
        std::runtime_error);

    // If `bad` had stayed registered, this add would propagate to a dangling view (UB / crash).
    auto fp = pool.new_filter();
    EXPECT_EQ(fp.size(), 2u);
    const auto id = fp.add(make_pool_solution(9.0, {{2, 1.0L}}, {30, 31}));
    EXPECT_NE(id, SolutionPool::kNoId);
    EXPECT_EQ(fp.size(), 3u);
}

// Concurrent adds (each via its own view) and reads on a shared unfiltered view: every distinct
// column must reach the base view exactly once, and size() must agree with get_all().
TEST(SolutionPoolConcurrency, ConcurrentAddsAndReadsCountInvariant) {
    SolutionPool pool;
    auto base = pool.new_filter();  // no filter — receives every column via propagation

    constexpr int kWriters = 4;
    constexpr int kPerWriter = 100;
    std::atomic<size_t> next_arc{1000};
    std::atomic<bool> go{false};
    std::atomic<bool> stop_readers{false};

    std::vector<std::thread> writers;
    for (int t = 0; t < kWriters; ++t) {
        writers.emplace_back([&] {
            auto view = pool.new_filter();  // each writer drives its own view
            while (!go.load()) {            // start together to maximise contention
            }
            for (int i = 0; i < kPerWriter; ++i) {
                const size_t arc =
                    next_arc.fetch_add(1);  // distinct arc ⇒ distinct path ⇒ no dedup
                view.add(make_pool_solution(1.0, {{0, 1.0L}}, {arc}));
                (void)view.price({0.5});
                (void)view.size();
            }
        });
    }

    std::vector<std::thread> readers;
    for (int r = 0; r < 2; ++r) {
        readers.emplace_back([&] {
            while (!stop_readers.load()) {
                (void)base.size();     // unsynchronised list-size read before C-3
                (void)base.get_all();  // racing iteration of filtered_entries_
                (void)base.pricing_count();
            }
        });
    }

    go.store(true);
    for (auto& w : writers) {
        w.join();
    }
    stop_readers.store(true);
    for (auto& rd : readers) {
        rd.join();
    }

    EXPECT_EQ(base.size(), static_cast<size_t>(kWriters * kPerWriter));
    EXPECT_EQ(base.size(), base.get_all().size());
}

// Several threads run local remove_if concurrently on the SAME view (disjoint id residue classes),
// while another reads it. With removers under an exclusive lock this is race-free and empties the
// view; under the old shared-lock-while-mutating bug it would corrupt the view's containers.
TEST(SolutionPoolConcurrency, ConcurrentLocalRemoveIfOnSharedView) {
    SolutionPool pool;
    auto v = pool.new_filter();
    constexpr int kN = 600;
    for (int i = 0; i < kN; ++i) {
        v.add(make_pool_solution(1.0, {{0, 1.0L}}, {static_cast<size_t>(7000 + i)}));
    }

    constexpr int kRemovers = 3;
    std::atomic<bool> go{false};
    std::atomic<bool> stop_reader{false};
    std::vector<std::thread> removers;
    for (int t = 0; t < kRemovers; ++t) {
        removers.emplace_back([&, t] {
            while (!go.load()) {
            }
            v.remove_if([t](SolutionPool::ColumnId cid, const Solution&, const ColumnActivity&) {
                return (cid % kRemovers) == static_cast<SolutionPool::ColumnId>(t);  // disjoint
            });
        });
    }
    std::thread reader([&] {
        while (!stop_reader.load()) {
            (void)v.size();
            (void)v.get_all();
        }
    });

    go.store(true);
    for (auto& th : removers) {
        th.join();
    }
    stop_reader.store(true);
    reader.join();

    // Residue classes 0..kRemovers-1 cover every id (1..kN) ⇒ all removed.
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.get_all().size(), 0u);
}

// Building filtered views concurrently with adds: each view must contain only entries that pass
// its filter (the constructor evaluates the filter off-lock, then applies under the lock).
TEST(SolutionPoolConcurrency, ConcurrentNewFilterDuringAddsRespectsFilter) {
    SolutionPool pool;
    constexpr int kWriters = 3;
    constexpr int kPerWriter = 150;
    std::atomic<size_t> next_arc{1};
    std::atomic<bool> go{false};
    std::atomic<bool> stop_ctor{false};
    std::atomic<bool> violation{false};
    std::atomic<int> views_built{0};

    const auto cost_filter = [](const Solution& s) { return s.column.cost < 100.0; };

    std::vector<std::thread> writers;
    for (int t = 0; t < kWriters; ++t) {
        writers.emplace_back([&] {
            auto w = pool.new_filter();
            while (!go.load()) {
            }
            for (int i = 0; i < kPerWriter; ++i) {
                const size_t arc = next_arc.fetch_add(1);
                w.add(make_pool_solution(static_cast<double>(arc), {{0, 1.0L}}, {arc}));
            }
        });
    }

    std::thread ctor([&] {
        while (!go.load()) {
        }
        while (!stop_ctor.load()) {
            auto view = pool.new_filter(cost_filter);
            for (const auto& [id, sol, act] : view.get_all()) {
                if (!(sol.column.cost < 100.0)) {
                    violation.store(true);
                }
            }
            views_built.fetch_add(1);
            std::this_thread::yield();
        }
    });

    go.store(true);
    for (auto& w : writers) {
        w.join();
    }
    stop_ctor.store(true);
    ctor.join();

    EXPECT_FALSE(violation.load())
        << "a concurrently-built view contained an entry failing its filter";
    EXPECT_GT(views_built.load(), 0);

    // Built after all adds: exactly the columns with cost < 100 (arcs 1..99).
    auto final_view = pool.new_filter(cost_filter);
    EXPECT_EQ(final_view.size(), 99u);
    for (const auto& [id, sol, act] : final_view.get_all()) {
        EXPECT_LT(sol.column.cost, 100.0);
    }
}
