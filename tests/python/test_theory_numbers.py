"""The numbers the theory pages state (docs/theory/), recomputed with rcspp.

Each test recomputes one section's numbers with ``examples/python/theory`` and compares them with
the values written on the page. A failure means the library's behaviour changed and the page
needs updating, or the page is wrong.
"""

import os
import sys

import pytest

_here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_here, "..", "..", "python", "src"))
sys.path.insert(0, os.path.join(_here, "..", "..", "examples", "python"))

from theory import page1, page2, page3, page4, page5, page6  # noqa: E402
from theory.toy import SINK  # noqa: E402


def approx(values):
    """``pytest.approx`` for a number or a list of numbers."""
    return pytest.approx(values, abs=1e-6)


def customers_of(path):
    """The customers a path visits, in order."""
    return [n for n in path if n not in (0, SINK)]


@pytest.fixture(scope="module")
def p1():
    """Page 1's numbers."""
    return page1.numbers()


@pytest.fixture(scope="module")
def p3():
    """Page 3's numbers."""
    return page3.numbers()


@pytest.fixture(scope="module")
def p4():
    """Page 4's numbers."""
    return page4.numbers()


@pytest.fixture(scope="module")
def p5():
    """Page 5's numbers."""
    return page5.numbers()


@pytest.fixture(scope="module")
def p6():
    """Page 6's numbers."""
    return page6.numbers()


# --- Page 1: master problem, pricing and duals -------------------------------------------------


def test_page1_routes_and_integer_plans(p1):
    """Sections 1.3 and 1.4: route costs, every plan, and the best one."""
    assert p1["route_costs"] == {
        "p_1": 20,
        "p_2": 20,
        "p_3": 20,
        "p_12": 24,
        "p_13": 25,
        "p_23": 26,
    }
    assert sorted(p1["integer_plans"].values()) == [44, 45, 46, 60]
    assert p1["integer_optimum"] == 44


def test_page1_route_counts(p1):
    """Section 1.4: how fast the number of routes grows."""
    counts = p1["route_counts"]
    assert counts[3] == 15 and counts[5] == 325 and counts[10] == 9_864_100
    assert f"{counts[25]:.1e}" == "4.2e+25" and f"{counts[100]:.1e}" == "2.5e+158"


def test_page1_master_lp_and_duals(p1):
    """Sections 1.5 and 1.6: the LP optimum, its solution and its duals."""
    assert p1["lp_value"] == approx(37.5)
    assert p1["lp_solution"] == approx({"p_12": 0.5, "p_13": 0.5, "p_23": 0.5})
    assert p1["lp_duals"] == approx([11.5, 12.5, 13.5])


def test_page1_column_generation(p1):
    """Section 1.10: the loop by hand, with HiGHS's duals."""
    rows = [
        (it["columns"], it["value"], it["duals"], it["best_reduced_cost"], it["added"])
        for it in p1["column_generation"]
    ]
    assert rows == [
        (3, approx(60), approx([20, 20, 20]), approx(-16), "p_12"),
        (4, approx(44), approx([20, 4, 20]), approx(-15), "p_13"),
        (5, approx(44), approx([4, 20, 20]), approx(-14), "p_23"),
        (6, approx(37.5), approx([11.5, 12.5, 13.5]), approx(0), None),
    ]
    assert p1["early_lower_bound_iteration_0"] == approx(12)


def test_page1_exercises(p1):
    """Check yourself 2 and 3."""
    assert p1["exercise_2_reduced_costs"] == approx({"p_12": 0, "p_13": 0, "p_23": -13})
    assert p1["exercise_3_lp"] == approx(12)
    assert p1["exercise_3_ip"] == approx(12)


# --- Page 2: resources and labels --------------------------------------------------------------


def test_page2_search_by_hand():
    """Section 2.6: the counts of the hand trace, and what the library returns."""
    n = page2.numbers()
    assert n["labels_created"] == 13
    assert n["extensions_infeasible"] == 8
    assert n["labels_dominated"] == 4
    assert n["routes_without_dominance"] == 7
    trace = sorted((values[0], customers_of(path)) for path, values in n["trace_at_sink"])
    assert trace == [(-16, [1, 2]), (-15, [1, 3]), (0, [1])]
    assert [(c, customers_of(p)) for c, p in n["library_at_sink"]] == trace
    assert [(c, customers_of(p)) for c, p in n["library_negative"]] == trace[:2]


# --- Page 3: dominance and elementarity --------------------------------------------------------


def test_page3_cycle_and_lp_bounds(p3):
    """Sections 3.4 and 3.6: the cycle, and the LP bound of each set of paths."""
    assert p3["cycle_load"] == 13 and p3["cycle_cost"] == 28
    assert p3["lp"]["revisits allowed"]["value"] == approx(35)
    assert p3["lp"]["revisits allowed"]["solution"] == {
        "0→1→2→1→0′": pytest.approx(1 / 3),
        "0→1→3→0′": pytest.approx(1 / 3),
        "0→2→3→0′": pytest.approx(2 / 3),
    }
    assert p3["lp"]["ng-paths"]["value"] == approx(37.5)
    assert p3["lp"]["elementary"]["value"] == approx(37.5)


def test_page3_pricing_at_final_duals(p3):
    """Section 3.4: the cycle prices at -7.5; with a memory, nothing is negative."""
    cost, path = p3["final_duals_best"]["revisits allowed"]
    assert cost == approx(-7.5) and path == [0, 1, 2, 1, SINK]
    assert p3["final_duals_best"]["ng-paths"][0] == approx(0)
    assert p3["final_duals_best"]["elementary"][0] == approx(0)


def test_page3_labels_at_sink(p3):
    """Section 3.5: elementary pricing leaves more labels at the sink."""
    assert p3["labels_at_sink"]["revisits allowed"] == 3
    assert p3["labels_at_sink"]["elementary"] == 6


def test_page3_ng_memory(p3):
    """Section 3.6, its note, and check yourself 4: the ng memory."""
    assert [step["M"] for step in p3["ng_walk"]] == [[2], [1, 2], [1, 3]]
    assert [step["S"] for step in p3["ng_walk"]] == [[], [2], [1]]
    assert p3["exercise_4"] == {"0→1→3→1": False, "0→1→2→3→1": False}


# --- Page 4: algorithms and preprocessing ------------------------------------------------------


def test_page4_exact_algorithms_agree(p4):
    """Section 4.2: the four labelling algorithms return the same routes."""
    for status, found in p4["exact"].values():
        assert status == "complete"
        assert [c for c, _ in found] == approx([-16, -15])
    assert p4["cycle_at_final_duals"] == approx(-16)


def test_page4_trading_exactness_for_speed(p4):
    """Sections 4.3 and 4.4: truncation, phases and the dives."""
    costs = {k: [c for c, _ in v] for k, v in p4["truncation"].items()}
    assert costs["default"] == approx([-16, -15])
    assert costs["num_labels_to_extend_by_node = 1"] == approx([-15])
    assert costs["num_labels_to_extend_by_node = 1, num_max_phases = 3"] == approx([-16, -15])
    assert [c for c, _ in p4["greedy"]] == approx([-16])
    assert sorted((c, customers_of(p)) for c, p in p4["tabu"]) == [
        (-16, [1, 2]),
        (-15, [1, 3]),
        (-15, [3, 1]),
    ]


def test_page4_known_issue(p4):
    """Section 4.5: a truncated search returns nothing, yet reports complete.

    If the status changes, the bug is fixed: update the warning on the page.
    """
    status, found = p4["known_issue"]["exact"]
    assert status == "complete" and [c for c, _ in found] == approx([-0.5])
    assert p4["known_issue"]["truncated"] == ("complete", [])


def test_page4_preprocessing(p4):
    """Section 4.6: feasibility preprocessing, and the shortest-path bound."""
    assert p4["feasibility_removed"] == ["2→1", "2→3"]
    assert p4["arcs_before_and_after"] == (12, 10)
    assert p4["shortest_path_bound"] == {"1→3": 25, "1→2": 24}


# --- Page 5: searching from both ends ----------------------------------------------------------


def test_page5_backward_search(p5):
    """Section 5.2: forward and backward searches, and two backward steps."""
    assert [(c, customers_of(p)) for c, p in p5["forward"]] == [(-16, [1, 2]), (-15, [1, 3])]
    assert [(c, customers_of(p)) for c, p in p5["backward"]] == [(-16, [1, 2]), (-14, [3, 2])]
    assert p5["deadline_steps"] == {"3→0′ back along 1→3": 13, "3→0′ back along 2→3": 12}


def test_page5_half_way_point(p5):
    """Section 5.4: the routes, and where they come from, for each half-way point."""
    runs = {
        h: ([(c, customers_of(p)) for c, p in run["routes"]], run["bounded"], run["joined"])
        for h, run in p5["half_way"].items()
    }
    assert runs[16.0] == ([(-16, [1, 2]), (-15, [1, 3]), (-14, [3, 2])], True, 1)
    assert runs[100.0] == ([(-16, [1, 2]), (-15, [1, 3])], True, 0)
    costs, bounded, _ = runs[0.0]
    assert [c for c, _ in costs] == approx([-16, -15, -15, -14]) and not bounded


def test_page5_cost_is_not_a_clock(p5):
    """Section 5.4: the library turns the split off for a cost clock, and says why."""
    assert not p5["cost_clock"]["bounded"]
    assert "threshold" in p5["cost_clock"]["reason"]


def test_page5_memories_at_the_join(p5):
    """Section 5.5: without a memory the join builds the cycle; with ng it does not."""
    assert p5["ng_join"]["no memory"] == (approx(-32), [0, 1, 2, 1, SINK])
    cost, path = p5["ng_join"]["ng memory"]
    assert cost == approx(-16) and sorted(customers_of(path)) == [1, 2]


def test_page5_model_checks(p5):
    """Sections 5.6 and 5.8: the model checks, the presets, a union visited set."""
    assert p5["check_model"] == [] and p5["presets_check_model"] == []
    assert [(c, customers_of(p)) for c, p in p5["presets_routes"]] == [
        (-16, [1, 2]),
        (-15, [1, 3]),
        (-14, [3, 2]),
    ]
    assert len(p5["union_set_problems"]) == 1
    assert "BackwardKind::ArcEndpoints" in p5["union_set_problems"][0]


# --- Page 6: from LP to integer solutions ------------------------------------------------------


def test_page6_root(p6):
    """Introduction and 6.1: the legal routes, the root LP and IP, the gap."""
    assert p6["routes"] == {
        "0→1→0′": 20,
        "0→2→0′": 20,
        "0→3→0′": 20,
        "0→1→2→0′": 24,
        "0→1→3→0′": 25,
        "0→3→1→0′": 25,
        "0→3→2→0′": 26,
    }
    assert p6["root_lp"] == approx(37.5)
    assert set(p6["root_solution"]) == {"0→1→2→0′", "0→1→3→0′", "0→3→2→0′"}
    assert p6["root_ip"] == approx(44)
    assert f"{p6['gap']:.1%}" == "14.8%"


def test_page6_counterexample(p6):
    """Section 6.2: the restricted master IP misses the optimum."""
    ce = p6["counterexample"]
    assert ce["added"] == [("{1,2}", 17), ("{1,3,4}", 35), ("{2,3,4}", 28)]
    assert ce["lp"] == approx(40) and ce["lp_solution"] == [0.5, 0.5, 0.5]
    assert ce["rmp_ip"] == approx(43) and sorted(ce["rmp_ip_plan"]) == ["{1,2}", "{3}", "{4}"]
    assert ce["true_optimum"] == approx(40) and sorted(ce["true_plan"]) == ["{1,2}", "{3,4}"]
    assert ce["final_duals"] == approx([12, 5, 9, 14])
    assert ce["reduced_cost_34"] == approx(0)


def test_page6_branching(p6):
    """Section 6.4: the root arc flows, and the two branches on arc 1 → 2."""
    assert p6["root_flows"] == approx(
        {"0→1": 1, "2→0′": 1, "0→3": 0.5, "3→0′": 0.5, "1→2": 0.5, "1→3": 0.5, "3→2": 0.5}
    )
    a = p6["branch_a"]
    assert a["lp"] == approx(44) and a["integer"] and not a["uses_artificial"]
    assert sorted(a["solution"]) == ["{1,2}", "{3}"]
    assert p6["branch_b"]["lp"] == approx(45) and not p6["branch_b"]["uses_artificial"]


def test_page6_vehicle_count_cut(p6):
    """Section 6.5: the cut closes the gap at the root."""
    cut = p6["cut"]
    assert cut["lp"] == approx(44) and sorted(cut["solution"]) == ["{1,2}", "{3}"]
    assert cut["duals"] == approx([4, 4, 4]) and cut["sigma"] == approx(16)


def test_reports_print():
    """Every page's report runs, as ``python -m theory`` prints it."""
    for page in (page1, page2, page3, page4, page5, page6):
        assert page.report().startswith("Page ")
