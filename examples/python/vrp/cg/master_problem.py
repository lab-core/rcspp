#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import mip
from vrp.cg.mp_solution import MPSolution

# Probe the best available solver once at import time.
_SOLVER: str = ""


def _probe_solver() -> str:
    global _SOLVER
    if _SOLVER:
        return _SOLVER
    try:
        m = mip.Model(solver_name=mip.GRB)
        m.verbose = 0
        _SOLVER = mip.GRB
    except Exception:
        _SOLVER = mip.CBC
    print(f"[MasterProblem] solver: {_SOLVER}")
    return _SOLVER


def _make_model(name: str = "") -> mip.Model:
    m = mip.Model(solver_name=_probe_solver(), name=name)
    m.verbose = 0
    return m


class MasterProblem:
    """Set-partitioning master problem for column generation.

    The LP relaxation model is built once from the first call to solve() and
    kept alive across iterations.  New columns are added in-place via
    mip.Column (which works correctly after an initial normal build).
    The final IP solve flips var types to BINARY on the same model.

    Usage:
        master = MasterProblem(demand_customer_ids)
        master.add_paths(initial_paths)
        while not_converged:
            sol = master.solve(relax=True)   # LP relaxation → duals
            master.add_paths(new_paths)      # added to model via mip.Column
        sol = master.solve(relax=False)      # final integer solve
    """

    def __init__(self, node_ids):
        self.node_ids_ = list(node_ids)
        # Column cache: (path_id, cost, {node_id: coefficient})
        self._columns: list[tuple[int, float, dict]] = []
        self._column_ids: set = set()
        # Persistent LP model (None until first solve)
        self._model: mip.Model | None = None
        self._constrs: dict[int, mip.Constr] = {}
        self._path_vars: dict[int, mip.Var] = {}

    def add_paths(self, paths) -> None:
        """Register new routes; add to model if already built."""
        for path in paths:
            if path.id in self._column_ids:
                continue
            visit_counts = {}
            for nid in path.visited_nodes:
                visit_counts[nid] = visit_counts.get(nid, 0) + 1
            coeffs = {
                nid: float(visit_counts[nid]) for nid in self.node_ids_ if nid in visit_counts
            }
            self._columns.append((path.id, path.cost, coeffs))
            self._column_ids.add(path.id)
            if self._model is not None:
                self._add_column(path.id, path.cost, coeffs)

    def solve(self, relax: bool = False) -> MPSolution:
        """Optimise the master problem.

        Args:
            relax: True → LP relaxation (returns dual values);
                   False → integer program (flips var types in-place).
        """
        if self._model is None:
            self._build_model()
        if not relax:
            self._set_var_types(mip.BINARY)
        self._model.optimize()
        sol = self._extract(dual=relax)
        if not relax:
            self._set_var_types(mip.CONTINUOUS)
        return sol

    def _build_model(self) -> None:
        """Build LP model from current column cache using mip.xsum."""
        m = _make_model("master_problem")
        for pid, cost, _ in self._columns:
            self._path_vars[pid] = m.add_var(
                name=f"y_{pid}", lb=0.0, obj=cost, var_type=mip.CONTINUOUS
            )
        for nid in self.node_ids_:
            terms = [
                coeffs[nid] * self._path_vars[pid]
                for pid, _, coeffs in self._columns
                if nid in coeffs
            ]
            self._constrs[nid] = m.add_constr(mip.xsum(terms) == 1.0, name=f"c_{nid}")
        self._model = m

    def _add_column(self, pid: int, cost: float, coeffs: dict) -> None:
        """Add one variable to the existing model via mip.Column."""
        col_constrs = [self._constrs[nid] for nid in self.node_ids_ if nid in coeffs]
        col_coeffs = [coeffs[nid] for nid in self.node_ids_ if nid in coeffs]
        self._path_vars[pid] = self._model.add_var(
            name=f"y_{pid}",
            lb=0.0,
            obj=cost,
            var_type=mip.CONTINUOUS,
            column=mip.Column(col_constrs, col_coeffs),
        )

    def _set_var_types(self, var_type: str) -> None:
        for var in self._path_vars.values():
            var.var_type = var_type

    def _extract(self, dual: bool) -> MPSolution:
        solution = MPSolution()
        ok = (mip.OptimizationStatus.OPTIMAL, mip.OptimizationStatus.FEASIBLE)
        if self._model.status not in ok:
            print(f"[MasterProblem] no solution (status={self._model.status})")
            return solution

        solution.cost = self._model.objective_value
        print(f"solution.cost={solution.cost}")

        for path_id, var in self._path_vars.items():
            solution.value_by_var_id[path_id] = var.x

        if dual:
            for node_id, constr in self._constrs.items():
                solution.dual_by_var_id[node_id] = constr.pi

        return solution
