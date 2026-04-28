#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

from datetime import date
from vrp.cg.mp_solution import MPSolution
from vrp.cg.path import Path


def format_solution(
    solution: MPSolution,
    paths: list[Path],
    depot_id: int,
    instance_name: str = "N/A",
    author: str = "N/A",
    reference: str = "N/A",
    solution_date: str | None = None,
    epsilon: float = 1e-6,
) -> str:
    """
    Format a VRP solution in the Solomon benchmark style.

    Parameters
    ----------
    solution      : MPSolution returned by VRP.solve()
    paths         : list of Path objects (VRP._VRP__paths)
    depot_id      : id of the depot node (excluded from route display)
    instance_name : name of the Solomon instance, e.g. "R101"
    author        : name of the person who produced this solution
    reference     : bibliographic reference or "N/A"
    solution_date : date string "YY-MM-DD"; defaults to today if None
    epsilon       : threshold above which a path variable is considered active

    Returns
    -------
    Formatted string ready to be printed or written to a file.

    Example
    -------
    >>> text = format_solution(sol, vrp._VRP__paths, vrp.depot_id_,
    ...                        instance_name="R101", author="bauke")
    >>> print(text)
    """
    if solution_date is None:
        solution_date = date.today().strftime("%y-%m-%d")

    # Index paths by id for fast lookup
    path_by_id = {p.id: p for p in paths}

    # Collect active routes (path value > epsilon), sorted for determinism
    active_paths = [
        path_by_id[path_id]
        for path_id, value in solution.value_by_var_id.items()
        if isinstance(path_id, int) and value > epsilon and path_id in path_by_id
    ]
    def first_customer(path: Path) -> int:
        for node_id in path.visited_nodes:
            if node_id != depot_id:
                return node_id
        return path.id  # fallback si la route est vide (ne devrait pas arriver)

    active_paths.sort(key=first_customer)

    lines = [
        f"Instance name : {instance_name}",
        f"Authors       : {author}",
        f"Date          : {solution_date}",
        f"Reference     : {reference}",
        "Solution",
    ]

    for route_idx, path in enumerate(active_paths, start=1):
        # Strip depot from both ends for display
        customers = [
            node_id
            for node_id in path.visited_nodes
            if node_id != depot_id
        ]
        route_str = " ".join(str(c) for c in customers)
        lines.append(f"Route {route_idx} : {route_str}")

    return "\n".join(lines)


def print_solution(
    solution: MPSolution,
    paths: list[Path],
    depot_id: int,
    **kwargs,
) -> None:
    """Convenience wrapper that prints the formatted solution directly."""
    print(format_solution(solution, paths, depot_id, **kwargs))