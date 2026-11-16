"""Case study, crew scheduling: the numbers of docs/theory/case-studies/crew-scheduling.md.

Sources, and what this case study takes from them:

- J.E. Beasley and B. Cao, "A tree search algorithm for the crew scheduling problem", European
  Journal of Operational Research 94 (1996) 517-526. The problem: "assigning K crews to N tasks",
  each task with a fixed start and finish time, "such that each crew does not exceed a limit on the
  total time it can spend working"; a crew may go from task i to task j only along an allowed
  transition, which has a cost. We follow it: the limit is on the sum of the durations of the tasks
  a crew does, K is given, and only the transitions cost anything.
- J.E. Beasley and B. Cao, "A dynamic programming based algorithm for the crew scheduling problem",
  Computers & Operations Research 25 (1998) 567-582, which states the problem as finding "K time
  limit constrained vertex disjoint paths which visit all vertices". We follow it: every task is
  done exactly once, by exactly K crews.
- OR-Library, crew scheduling, https://people.brunel.ac.uk/~mastjjb/jeb/orlib/cspinfo.html: the
  instances of the 1996 paper, csp50 to csp500, under the MIT licence
  (https://people.brunel.ac.uk/~mastjjb/jeb/orlib/legal.html). We use none of them: the toy below
  is ours, small enough to check by hand. Their files list the allowed transitions; ours follow one
  rule, a crew waits at most 3 hours between two tasks, and a transition costs the hours waited.
  Theirs also go forward in time, which is what the page relies on. While preparing the
  page, the same model was run on csp50 with rcspp (built-in resources only, about 1 ms per pricing
  call); that run is not part of this package.
"""

from __future__ import annotations

from itertools import combinations

import numpy as np
from scipy.optimize import Bounds, LinearConstraint, linprog, milp

from rcspp import presets
from rcspp.graph import AlgorithmParams, ResourceGraph

from . import toy

TASKS = {1: (0, 4), 2: (2, 6), 3: (3, 5), 4: (4, 8), 5: (8, 10), 6: (10, 12)}  # hours
LIMIT = 6  # hours of work per crew: the sum of the durations of its tasks
CREWS = 4  # K
MAX_WAIT = 3  # hours a crew may wait between two tasks
# A crew can go from task i to task j when j starts 0 to MAX_WAIT hours after i finishes; the
# connection costs the hours waited.
CONNECTIONS = {
    (i, j): TASKS[j][0] - TASKS[i][1]
    for i in TASKS
    for j in TASKS
    if 0 <= TASKS[j][0] - TASKS[i][1] <= MAX_WAIT
}
SOURCE, SINK = 0, 7
CREW_ROW = len(TASKS)  # rows 0..5: task 1..6; row 6: the number of crews
START = [(1,), (2, 5), (3,), (4, 6)]  # a plan by hand, cost 4: the first columns
HALF_WAY = 6.0  # the middle of the day, for the search from both ends


def duration(task: int) -> int:
    start, finish = TASKS[task]
    return finish - start


def cost(duty) -> float:
    return float(sum(CONNECTIONS[i, j] for i, j in zip(duty, duty[1:])))


def duties() -> list[tuple[int, ...]]:
    """Every duty a crew can do: a chain of connections, within the limit."""
    out = []

    def grow(duty, work):
        out.append(tuple(duty))
        for (i, j) in CONNECTIONS:
            if i == duty[-1] and work + duration(j) <= LIMIT:
                grow(duty + [j], work + duration(j))

    for task in TASKS:
        grow([task], duration(task))
    return sorted(out)


def column(duty):
    """The duty's column: a 1 in the row of each task it does, and in the crew row."""
    a = np.zeros(len(TASKS) + 1)
    for task in duty:
        a[task - 1] = 1
    a[CREW_ROW] = 1
    return a


def master(columns, crews: int = CREWS, integer: bool = False):
    """Cover every task exactly once with exactly ``crews`` crews. None when infeasible."""
    a = np.column_stack([column(d) for d in columns])
    b = np.ones(len(TASKS) + 1)
    b[CREW_ROW] = crews
    costs = [cost(d) for d in columns]
    if integer:
        res = milp(costs, constraints=LinearConstraint(a, b, b), integrality=np.ones(len(costs)),
                   bounds=Bounds(0, 1))
        return None if res.status != 0 else (res.fun, list(res.x), None)
    res = linprog(costs, A_eq=a, b_eq=b, bounds=(0, None), method="highs")
    return None if res.status != 0 else (res.fun, list(res.x), list(res.eqlin.marginals))


def pricing_graph() -> ResourceGraph:
    """Nodes: the source, the tasks, the sink. Arcs: start a duty, connect, end a duty."""
    rg = ResourceGraph()
    presets.add_cost_resource(rg)  # resource 0: reduced cost
    presets.add_capacity_resource(rg, "real", float(LIMIT))  # resource 1: hours of work
    clock = {SOURCE: (0.0, 0.0), SINK: (0.0, 24.0)}
    clock.update({t: (float(s), float(s)) for t, (s, _) in TASKS.items()})
    presets.add_window_resource(rg, "real", clock)  # resource 2: the time of day
    rg.add_node(SOURCE, source=True)
    for task in TASKS:
        rg.add_node(task)
    rg.add_node(SINK, sink=True)

    def add(i, j, arc_cost):
        work = duration(j) if j in TASKS else 0  # an arc adds the work of the task it enters
        busy = duration(i) if i in TASKS else 0  # leaving i, the crew is free when i finishes
        rows = [(CREW_ROW, 1.0)] if i == SOURCE else [(i - 1, 1.0)]
        rg.add_arc((arc_cost, work, busy), i, j, cost=arc_cost, rows=rows)

    for task in TASKS:
        add(SOURCE, task, 0.0)
    for (i, j), arc_cost in CONNECTIONS.items():
        add(i, j, float(arc_cost))
    for task in TASKS:
        add(task, SINK, 0.0)
    return rg


def solve(rg: ResourceGraph, algorithm: str = "simple"):
    params = AlgorithmParams()
    if algorithm == "bidirectional":
        params.critical_resource_index = 2  # the clock, the third real resource
        params.half_way_point = HALF_WAY
    return rg.solve(algorithm=algorithm, upper_bound=-toy.EPSILON, params=params)


def found(result):
    """The duties a solve returns, as (reduced cost, tasks), cheapest first."""
    return [
        (round(s.cost, 9), tuple(n for n in s.path_node_ids if n in TASKS))
        for s in result.solutions
    ]


def column_generation(algorithm: str = "simple"):
    """From the plan START, add the best duty per iteration until pricing finds none."""
    rg = pricing_graph()
    columns = list(START)
    history = []
    while True:
        value, x, duals = master(columns)
        rg.update_reduced_costs(duals)
        result = solve(rg, algorithm)
        routes = found(result)
        history.append({
            "value": value,
            "duals": duals,
            "found": routes,
            "status": result.status_string(),
        })
        if not routes:
            used = {d: v for d, v in zip(columns, x) if v > toy.EPSILON}
            return history, columns, used
        columns.append(routes[0][1])


def has_cycle() -> bool:
    """Whether the connections contain a cycle (they cannot: each one goes forward in time)."""
    succ = {}
    for i, j in CONNECTIONS:
        succ.setdefault(i, []).append(j)
    state = {}

    def visit(n):
        state[n] = "open"
        for m in succ.get(n, []):
            if state.get(m) == "open" or (m not in state and visit(m)):
                return True
        state[n] = "done"
        return False

    return any(n not in state and visit(n) for n in TASKS)


def numbers() -> dict:
    """Compute every number the case study states."""
    out: dict = {}

    # 1: the instance
    out["connections"] = dict(CONNECTIONS)
    out["work"] = sum(duration(t) for t in TASKS)
    out["forward_in_time"] = all(TASKS[i][1] <= TASKS[j][0] for i, j in CONNECTIONS)
    out["running_at_4"] = [t for t, (s, f) in TASKS.items() if s <= 4 < f]
    out["has_cycle"] = has_cycle()

    # 2: the duties, the plans and the number of crews
    all_duties = duties()
    out["duties"] = {d: cost(d) for d in all_duties}
    out["feasible_crews"] = [
        k for k in range(1, len(TASKS) + 1) if master(all_duties, k) is not None
    ]
    out["cost_with_5_crews"] = master(all_duties, 5, integer=True)[0]
    plans = []
    for chosen in combinations(all_duties, CREWS):
        if sorted(t for d in chosen for t in d) == list(TASKS):
            plans.append((sum(cost(d) for d in chosen), list(chosen)))
    out["plans"] = sorted(plans)

    # 3: the pricing graph is accepted for a search from both ends
    out["check_model"] = pricing_graph().check_model("bidirectional")

    # 4: column generation from the plan of cost 4
    history, columns, used = column_generation()
    out["column_generation"] = history
    out["lp_solution"] = used
    final = history[-1]["duals"]
    out["final_reduced_costs"] = {
        d: cost(d) - sum(final[t - 1] for t in d) - final[CREW_ROW] for d in all_duties
    }
    # the same first pricing call with the four labelling algorithms
    rg = pricing_graph()
    rg.update_reduced_costs(history[0]["duals"])
    out["algorithms_agree"] = {
        a: found(solve(rg, a)) for a in ("simple", "pushing", "pulling", "astar")
    }

    # 5: from the LP to a plan
    out["lp_over_all_duties"] = master(all_duties)[0]
    out["integer_optimum"] = master(all_duties, integer=True)[0]
    value, x, _ = master(columns, integer=True)
    out["integer_over_generated"] = (value, [d for d, v in zip(columns, x) if v > 0.5])
    out["solo_4_generated"] = (4,) in columns
    out["solo_4_returned_at"] = [
        i for i, it in enumerate(history) if any(d == (4,) for _, d in it["found"])
    ]

    # 6: searching from both ends, with the time of day as the clock
    both, _, _ = column_generation("bidirectional")

    def path(h):
        return [(it["value"], it["found"][0][1] if it["found"] else None) for it in h]

    out["bidirectional_same"] = path(both) == path(history)
    rg = pricing_graph()
    rg.update_reduced_costs(history[0]["duals"])
    out["bidirectional_split"] = solve(rg, "bidirectional").bidirectional.bounded_by_half_way
    return out


def report() -> str:
    """The case study's numbers, as text."""
    n = numbers()
    lines = ["Case study: crew scheduling (6 tasks, 6 hours of work per crew, 4 crews)", ""]
    lines.append(
        f"1 total work {n['work']} hours; connections forward in time: {n['forward_in_time']};"
        f" running at hour 4: {n['running_at_4']}; a cycle: {n['has_cycle']}"
    )
    lines.append(
        "2 duties: " + ", ".join(f"{_name(d)} ({c:g})" for d, c in n["duties"].items())
    )
    lines.append(
        f"  crew counts with a plan: {n['feasible_crews']}; best plan with 5 crews:"
        f" {n['cost_with_5_crews']:g}"
    )
    for c, plan in n["plans"]:
        lines.append(f"  plan of {CREWS} crews, cost {c:g}: " + " + ".join(_name(d) for d in plan))
    lines.append(f"3 check_model: {n['check_model']}")
    for i, it in enumerate(n["column_generation"]):
        best = f"{_name(it['found'][0][1])} at {it['found'][0][0]:g}" if it["found"] else "none"
        lines.append(
            f"4 iteration {i}: RMP {it['value']:g}, duals {_fmt(it['duals'])}, best {best},"
            f" {it['status']}"
        )
    lines.append(
        "  LP solution: " + ", ".join(f"{_name(d)} = {v:g}" for d, v in n["lp_solution"].items())
    )
    lines.append(
        "  reduced costs at the last duals: "
        + ", ".join(f"{_name(d)} {v:g}" for d, v in n["final_reduced_costs"].items())
    )
    lines.append(f"  first pricing call, four algorithms: {n['algorithms_agree']}")
    value, plan = n["integer_over_generated"]
    lines.append(
        f"5 LP over all duties {n['lp_over_all_duties']:g}, integer optimum"
        f" {n['integer_optimum']:g}; integer over the generated columns {value:g}"
        f" ({' + '.join(_name(d) for d in plan)}); duty (4) added: {n['solo_4_generated']},"
        f" returned at iterations {n['solo_4_returned_at']}"
    )
    lines.append(
        f"6 bidirectional, H = {HALF_WAY:g}: same iterations {n['bidirectional_same']},"
        f" split in force {n['bidirectional_split']}"
    )
    return "\n".join(lines)


def _name(duty) -> str:
    return "(" + ",".join(str(t) for t in duty) + ")"


def _fmt(values) -> str:
    return "(" + ", ".join(f"{v:g}" for v in values) + ")"
