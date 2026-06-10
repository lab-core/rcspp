#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../../python/src")))

from rcspp._core import solution_pool as _sp  # noqa: E402
from rcspp._core.graph import Column, Row, Solution  # noqa: E402

SolutionPool = _sp.SolutionPool
FilteredSolutionPool = _sp.FilteredSolutionPool


def make_solution(col_cost: float, rows: list[tuple[int, float]], arc_ids: list[int]) -> Solution:
    col = Column()
    col.cost = col_cost
    col.rows = [Row(index=i, coefficient=c) for i, c in rows]
    sol = Solution()
    sol.cost = col_cost
    sol.path_arc_ids = arc_ids
    sol.path_node_ids = list(range(len(arc_ids) + 1))
    sol.column = col
    return sol


# ── add / deduplication ───────────────────────────────────────────────────────


def test_add_deduplication():
    pool = SolutionPool()
    fp = pool.new_filter()
    s1 = make_solution(5.0, [(0, 1.0)], [10, 11])
    s2 = make_solution(5.0, [(0, 1.0)], [10, 11])  # identical to s1
    s3 = make_solution(7.0, [(1, 1.0)], [20, 21])

    id1 = fp.add(s1)
    id2 = fp.add(s2)
    id3 = fp.add(s3)

    assert id1 == id2, f"duplicate got different ids: {id1} vs {id2}"
    assert id1 != id3
    assert id1 != SolutionPool.NO_ID
    assert id3 != SolutionPool.NO_ID
    assert len(fp) == 2


def test_add_batch():
    pool = SolutionPool()
    fp = pool.new_filter()
    s1 = make_solution(5.0, [(0, 1.0)], [10, 11])
    s2 = make_solution(7.0, [(1, 1.0)], [20, 21])

    ids = fp.add([s1, s2, s1])

    assert len(fp) == 2
    assert len(ids) == 3
    assert ids[0] == ids[2], "duplicate s1 should return same id"


def test_duplicate_add_refreshes_column():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
    # Re-add the same arc path with an updated (cheaper) column; latest column must win.
    id2 = fp.add(make_solution(3.0, [(1, 2.0)], [10, 11]))
    assert id1 == id2  # deduped by path
    assert len(fp) == 1
    got = fp.get(id1)
    assert got is not None
    assert abs(got.column.cost - 3.0) < 1e-9  # was 5.0 before the refresh
    assert len(got.column.rows) == 1
    assert got.column.rows[0].index == 1
    assert abs(got.column.rows[0].coefficient - 2.0) < 1e-9


# ── get ───────────────────────────────────────────────────────────────────────


def test_get_by_id():
    pool = SolutionPool()
    fp = pool.new_filter()
    s1 = make_solution(5.0, [(0, 1.0)], [10, 11])
    id1 = fp.add(s1)

    retrieved = fp.get(id1)
    assert retrieved is not None
    assert abs(retrieved.column.cost - 5.0) < 1e-9

    assert fp.get(SolutionPool.NO_ID) is None


# ── price ─────────────────────────────────────────────────────────────────────


def test_price_threshold():
    pool = SolutionPool()
    fp = pool.new_filter()
    # col.cost=10, rows=[(0, 1)]; duals=[3] → rc=7 (not returned); duals=[11] → rc=-1 (returned)
    id1 = fp.add(make_solution(10.0, [(0, 1.0)], [0, 1]))

    r1 = fp.price([3.0])
    assert len(r1) == 0, f"expected empty for rc=7, got {len(r1)}"

    r2 = fp.price([11.0])
    assert len(r2) == 1
    assert r2[0].id == id1
    assert abs(r2[0].reduced_cost - (-1.0)) < 1e-9
    assert abs(r2[0].solution.column.cost - 10.0) < 1e-9, "price must not mutate stored cost"


def test_price_fractional_coefficients():
    pool = SolutionPool()
    fp = pool.new_filter()
    # col.cost=2.5, rows=[(0,0.1),(1,0.2)]; duals=[3,4] → rc = 2.5 - 0.3 - 0.8 = 1.4
    id1 = fp.add(make_solution(2.5, [(0, 0.1), (1, 0.2)], [10, 11]))
    r = fp.price([3.0, 4.0], 2.0)  # rc 1.4 < 2.0 → returned
    assert len(r) == 1 and r[0].id == id1
    assert abs(r[0].reduced_cost - 1.4) < 1e-9


def test_price_does_not_mutate_stored_cost():
    pool = SolutionPool()
    fp = pool.new_filter()
    fp.add(make_solution(6.0, [(0, 2.0), (1, 1.0)], [0, 1]))
    fp.price([1.0, 2.0])
    all_entries = fp.get_all()
    assert abs(all_entries[0][1].column.cost - 6.0) < 1e-9, "stored cost was mutated"


def test_price_out_of_range_dual():
    pool = SolutionPool()
    fp = pool.new_filter()
    fp.add(make_solution(8.0, [(5, 1.0)], [0, 1]))  # row index 5 but only 3 duals
    r = fp.price([1.0, 2.0, 3.0])
    assert len(r) == 0, "out-of-range dual treated as 0 → rc=8.0 > 0"


# ── activity tracking ─────────────────────────────────────────────────────────


def test_activity_tracking():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(10.0, [(0, 1.0)], [0, 1]))

    assert fp.pricing_count() == 0

    act = fp.get_activity(id1)
    assert act is not None and act.created_at == 0

    fp.price([3.0])  # rc=7 → not returned: age=1, use_count=0
    act = fp.get_activity(id1)
    assert act.age == 1 and act.use_count == 0 and not act.last_was_negative
    assert fp.pricing_count() == 1

    fp.price([11.0])  # rc=-1 → returned: age=0, use_count=1
    act = fp.get_activity(id1)
    assert act.age == 0 and act.use_count == 1 and act.last_was_negative
    assert abs(act.usage_rate() - 0.5) < 1e-9

    fp.price([3.0])
    fp.price([3.0])
    act = fp.get_activity(id1)
    assert act.age == 2 and act.use_count == 1
    assert abs(act.usage_rate() - 0.25) < 1e-9


# ── row/arc filters ───────────────────────────────────────────────────────────


def test_price_compulsory_rows():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(5.0, [(0, 1.0), (1, 1.0)], [10, 11]))
    fp.add(make_solution(3.0, [(2, 1.0)], [20, 21]))
    duals = [10.0, 10.0, 10.0]

    fp2 = pool.new_filter(compulsory_rows=[0])
    r = fp2.price(duals)
    assert len(r) == 1 and r[0].id == id1

    fp3 = pool.new_filter(compulsory_rows=[0, 2])
    assert len(fp3.price(duals)) == 0


def test_price_forbidden_rows():
    pool = SolutionPool()
    fp = pool.new_filter()
    fp.add(make_solution(5.0, [(0, 1.0), (1, 1.0)], [10, 11]))
    id2 = fp.add(make_solution(3.0, [(2, 1.0)], [20, 21]))
    duals = [10.0, 10.0, 10.0]

    fp2 = pool.new_filter(forbidden_rows=[1])
    r = fp2.price(duals)
    assert len(r) == 1 and r[0].id == id2

    fp3 = pool.new_filter(forbidden_rows=[0, 2])
    assert len(fp3.price(duals)) == 0


def test_price_arc_filters():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
    id2 = fp.add(make_solution(3.0, [(0, 1.0)], [20, 21]))
    duals = [10.0]

    fp2 = pool.new_filter(compulsory_arc_ids=[10])
    r = fp2.price(duals)
    assert len(r) == 1 and r[0].id == id1

    fp3 = pool.new_filter(forbidden_arc_ids=[10])
    r = fp3.price(duals)
    assert len(r) == 1 and r[0].id == id2

    fp4 = pool.new_filter(forbidden_arc_ids=[10, 20])
    assert len(fp4.price(duals)) == 0


# ── remove operations ─────────────────────────────────────────────────────────


def test_global_remove_if():
    pool = SolutionPool()
    fp = pool.new_filter()
    fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
    fp.add(make_solution(15.0, [(1, 1.0)], [20, 21]))
    fp.add(make_solution(8.0, [(2, 1.0)], [30, 31]))

    removed = fp.global_remove_if(lambda cid, sol, act: sol.column.cost > 10.0)
    assert len(removed) == 1
    assert len(fp) == 2
    for _, sol, _ in fp.get_all():
        assert sol.column.cost <= 10.0


def test_remove_if_arc_present():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
    id2 = fp.add(make_solution(7.0, [(1, 1.0)], [20, 21]))
    id3 = fp.add(make_solution(6.0, [(2, 1.0)], [10, 30]))

    removed = fp.global_remove_if_arc_present(10)
    assert len(removed) == 2
    assert len(fp) == 1
    assert fp.get(id2) is not None
    assert fp.get(id1) is None
    assert fp.get(id3) is None


def test_remove_stale_by_age():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))  # rc=5-6=-1 → returned
    id2 = fp.add(make_solution(8.0, [(0, 1.0)], [20, 21]))  # rc=8-6=2 → not returned

    fp.price([6.0])  # id1: age=0, id2: age=1
    fp.price([6.0])  # id1: age=0, id2: age=2

    removed = fp.global_remove_stale(max_age=1)
    assert len(removed) == 1 and removed[0] == id2
    assert len(fp) == 1
    assert fp.get(id1) is not None


def test_local_remove_arc_backtrack():
    pool = SolutionPool()
    fp_all = pool.new_filter()
    id1 = fp_all.add(make_solution(5.0, [(0, 1.0)], [10, 20]))
    id2 = fp_all.add(make_solution(7.0, [(1, 1.0)], [30, 40]))

    fp = pool.new_filter()
    removed = fp.remove_if_arc_present(10)
    assert len(removed) == 1 and removed[0] == id1
    assert len(fp) == 1 and fp.get(id2) is not None

    # Main pool must retain id1 (local remove does not affect pool)
    assert fp_all.get(id1) is not None


# ── update_activity ───────────────────────────────────────────────────────────


def test_update_activity():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
    id2 = fp.add(make_solution(8.0, [(1, 1.0)], [20, 21]))

    fp.update_activity([id1])
    act1 = fp.get_activity(id1)
    act2 = fp.get_activity(id2)
    # basis membership resets age and sets last_was_negative, but does NOT bump use_count
    assert act1.age == 0 and act1.use_count == 0 and act1.last_was_negative
    assert act2.age == 1 and act2.use_count == 0 and not act2.last_was_negative
    assert fp.pricing_count() == 0


# ── usage_rate semantics (L-1 / L-2 / L-4) ────────────────────────────────────


def test_usage_rate_not_inflated_by_basis():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(1.0, [(0, 1.0)], [10, 11]))
    fp.price([2.0])  # rc = 1 - 2 = -1 → returned: priced_count=1, use_count=1
    for _ in range(5):
        fp.update_activity([id1])  # basis membership must not bump use_count or inflate usage_rate
    act = fp.get_activity(id1)
    assert act.priced_count == 1
    assert act.use_count == 1
    assert abs(act.usage_rate() - 1.0) < 1e-9  # 1/1, never > 1


def test_remove_stale_keeps_never_priced_column():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(10.0, [(0, 1.0)], [10, 11]))
    fp.price([0.0])  # rc = 10 → priced but not returned (use_count=0)
    fp.price([0.0])
    id2 = fp.add(make_solution(5.0, [(1, 1.0)], [20, 21]))  # added after pricing → never priced
    removed = fp.global_remove_stale(max_age=1000, min_usage_rate=0.5)
    assert removed == [id1]  # priced & unused → evicted
    assert fp.get(id2) is not None  # never-priced column survives (priced_count == 0)
    assert len(fp) == 1


# ── auto-propagation ──────────────────────────────────────────────────────────


def test_autopropagation_add():
    pool = SolutionPool()
    fp1 = pool.new_filter()
    fp2 = pool.new_filter(forbidden_arc_ids=[10])

    id1 = fp1.add(make_solution(5.0, [(0, 1.0)], [20, 21]))
    assert fp1.get(id1) is not None and fp2.get(id1) is not None

    id2 = fp1.add(make_solution(7.0, [(1, 1.0)], [10, 11]))
    assert fp1.get(id2) is not None
    assert fp2.get(id2) is None  # forbidden arc 10


def test_autopropagation_remove():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
    id2 = fp.add(make_solution(7.0, [(1, 1.0)], [20, 21]))

    fp1 = pool.new_filter()
    fp2 = pool.new_filter()
    assert len(fp1) == 2 and len(fp2) == 2

    fp.global_remove_if(lambda cid, sol, act: cid == id1)
    assert fp1.get(id1) is None and fp2.get(id1) is None
    assert fp1.get(id2) is not None and fp2.get(id2) is not None


# ── chain filtering / new_filter ─────────────────────────────────────────────


def test_chain_filter():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
    id2 = fp.add(make_solution(6.0, [(0, 1.0)], [20, 21]))
    id3 = fp.add(make_solution(7.0, [(1, 1.0)], [10, 30]))

    fp1 = pool.new_filter(compulsory_rows=[0])
    assert len(fp1) == 2 and fp1.get(id1) and fp1.get(id2)

    fp2 = fp1.new_filter(forbidden_arc_ids=[10])
    assert len(fp2) == 1 and fp2.get(id2) and not fp2.get(id1) and not fp2.get(id3)

    id4 = fp.add(make_solution(4.0, [(0, 1.0)], [25, 26]))
    assert fp1.get(id4) is not None and fp2.get(id4) is not None


def test_add_filter_mutates_view():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
    id2 = fp.add(make_solution(6.0, [(0, 1.0)], [20, 21]))

    fp.add_filter(forbidden_arc_ids=[10])
    assert len(fp) == 1 and fp.get(id2) and not fp.get(id1)


# ── custom Python predicate ───────────────────────────────────────────────────


def test_custom_filter():
    pool = SolutionPool()
    id1 = pool.new_filter().add(make_solution(3.0, [(0, 1.0)], [10, 11]))
    id2 = pool.new_filter().add(make_solution(9.0, [(1, 1.0)], [20, 21]))

    fp = pool.new_filter(filter=lambda sol: sol.column.cost < 5.0)
    assert len(fp) == 1 and fp.get(id1) and not fp.get(id2)


# ── get_entry / get_all ───────────────────────────────────────────────────────


def test_get_entry_and_get_all():
    pool = SolutionPool()
    fp = pool.new_filter()
    id1 = fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))

    entry = fp.get_entry(id1)
    assert entry is not None
    eid, esol, eact = entry
    assert eid == id1
    assert abs(esol.column.cost - 5.0) < 1e-9

    all_entries = fp.get_all()
    assert len(all_entries) == 1
    assert all_entries[0][0] == id1


# ── Solution hashing (H-1) ────────────────────────────────────────────────────


def test_solution_hash_tracks_path_arc_ids():
    # The idiomatic Python build (default ctor + attribute assignment) must yield a content hash,
    # not the stale empty-path hash — otherwise SolutionPool's hash_index_ collapses to one bucket.
    empty_hash = Solution().get_hash()

    s1 = make_solution(5.0, [(0, 1.0)], [10, 11])
    s2 = make_solution(7.0, [(1, 1.0)], [10, 11])  # same arc path, different cost/rows
    s3 = make_solution(5.0, [(0, 1.0)], [20, 21])  # different arc path

    assert s1.get_hash() == s2.get_hash(), "same arc path → same hash"
    assert s1.get_hash() != s3.get_hash(), "different arc path → different hash (was all-0 before)"
    assert s1.get_hash() != empty_hash, "a populated solution must not keep the empty-path hash"


# ── PricedColumn lifetime (P-2) ───────────────────────────────────────────────


def test_priced_column_solution_survives_pool_removal():
    pool = SolutionPool()
    fp = pool.new_filter()
    fp.add(make_solution(5.0, [(0, 1.0)], [10, 11]))
    fp.add(make_solution(7.0, [(0, 1.0)], [20, 21]))

    # duals=[100] → rc = 5-100 and 7-100, both < 0 → both columns returned
    priced = fp.price([100.0])
    assert len(priced) == 2
    costs_before = sorted(pc.solution.column.cost for pc in priced)
    assert costs_before == [5.0, 7.0]

    # Hard-delete every column. PricedColumn.solution owns a copy taken at price() time, so the
    # already-returned results must stay valid — a borrowed pointer here would be a use-after-free.
    removed = fp.global_remove_if(lambda cid, sol, act: True)
    assert len(removed) == 2 and len(fp) == 0

    costs_after = sorted(pc.solution.column.cost for pc in priced)
    assert costs_after == costs_before
    for pc in priced:
        assert len(pc.solution.path_arc_ids) == 2  # path data preserved in the copy


# ── runner ────────────────────────────────────────────────────────────────────

_TESTS = [
    test_add_deduplication,
    test_add_batch,
    test_duplicate_add_refreshes_column,
    test_get_by_id,
    test_price_threshold,
    test_price_fractional_coefficients,
    test_price_does_not_mutate_stored_cost,
    test_price_out_of_range_dual,
    test_activity_tracking,
    test_price_compulsory_rows,
    test_price_forbidden_rows,
    test_price_arc_filters,
    test_global_remove_if,
    test_remove_if_arc_present,
    test_remove_stale_by_age,
    test_local_remove_arc_backtrack,
    test_update_activity,
    test_usage_rate_not_inflated_by_basis,
    test_remove_stale_keeps_never_priced_column,
    test_autopropagation_add,
    test_autopropagation_remove,
    test_chain_filter,
    test_add_filter_mutates_view,
    test_custom_filter,
    test_get_entry_and_get_all,
    test_solution_hash_tracks_path_arc_ids,
    test_priced_column_solution_survives_pool_removal,
]

if __name__ == "__main__":
    passed = 0
    for fn in _TESTS:
        try:
            fn()
            print(f"  PASS  {fn.__name__}")
            passed += 1
        except Exception as e:
            print(f"  FAIL  {fn.__name__}: {e}")
    print(f"\n{passed}/{len(_TESTS)} solution pool tests passed")
    if passed != len(_TESTS):
        sys.exit(1)
