#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The pure-Python column generation layer: its types and the contract of its master solver and
pricer."""

import ast
import math
import os
import subprocess
import sys
from dataclasses import FrozenInstanceError, replace
from typing import Any

import numpy as np
import pytest

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

import rcspp.column_generation as layer  # noqa: E402
from rcspp._core.graph import Column, Row, Solution  # noqa: E402
from rcspp.column_generation.types import (  # noqa: E402
    FloatArray,
    IdArray,
    OptionValue,
)


def make_path(cost: float, rows: list[tuple[int, float]], reduced_cost: float) -> Solution:
    """A solution as a pricer returns it."""
    column = Column()
    column.cost = cost
    column.rows = [Row(index=row, coefficient=coefficient) for row, coefficient in rows]
    solution = Solution()
    solution.cost = reduced_cost
    solution.column = column
    return solution


# MARK: Enums, constants and options


def test_enum_values() -> None:
    assert list(layer.SolverStatus.__members__) == [
        "Optimal",
        "Infeasible",
        "Unbounded",
        "TimeLimit",
        "Other",
    ]
    assert list(layer.RowSense.__members__) == ["Equal", "GreaterEqual", "LessEqual"]
    assert list(layer.ColumnType.__members__) == ["Integer", "Binary"]
    assert list(layer.PricingStatus.__members__) == [
        "Optimal",
        "SolverLimit",
        "Infeasible",
        "NotApplicable",
        "Unknown",
    ]
    assert str(layer.SolverStatus.TimeLimit) == "SolverStatus.TimeLimit"


def test_option_defaults_and_attributes() -> None:
    assert layer.DEFAULT_RELATIVE_GAP == 1e-4

    capabilities = layer.Capabilities()
    assert not (capabilities.add_rows or capabilities.integer_solve)
    assert not (capabilities.farkas or capabilities.basis)
    capabilities = layer.Capabilities(add_rows=True, basis=True)
    assert capabilities.add_rows and capabilities.basis and not capabilities.integer_solve

    with pytest.raises(TypeError):
        layer.Capabilities(True)  # type: ignore[call-arg]  # pyright: ignore[reportCallIssue]

    relaxation = layer.RelaxationParams()
    assert (relaxation.timeout_s, relaxation.threads, relaxation.verbose) == (math.inf, 0, False)
    assert layer.RelaxationParams(timeout_s=2.5, verbose=True).timeout_s == 2.5

    integer = layer.IntegerParams()
    assert integer.timeout_s == math.inf and integer.relative_gap == layer.DEFAULT_RELATIVE_GAP
    assert integer.column_type == layer.ColumnType.Integer
    assert integer.threads == 0 and not integer.verbose
    assert layer.IntegerParams(column_type=layer.ColumnType.Binary).column_type == (
        layer.ColumnType.Binary
    )

    request = layer.PricingRequest()
    assert request.reduced_cost_threshold == 0.0 and request.timeout_s == math.inf
    changes = layer.PricingChanges(costs=True)
    assert changes.costs and not changes.bounds and not changes.structure


def test_the_option_records_are_frozen() -> None:
    # A default instance is shared by every call that takes it, so none may change.
    records: list[Any] = [
        layer.Capabilities(),
        layer.RelaxationParams(),
        layer.IntegerParams(),
        layer.PricingRequest(),
        layer.PricingChanges(),
    ]

    for record in records:
        with pytest.raises(FrozenInstanceError):
            record.verbose = True

    relaxation = replace(layer.RelaxationParams(), threads=4)
    assert relaxation.threads == 4 and layer.RelaxationParams().threads == 0
    assert layer.IntegerParams(threads=2) == layer.IntegerParams(threads=2)


def test_a_pricing_result() -> None:
    result = layer.PricingResult()
    assert result.columns == []
    assert result.status == layer.PricingStatus.Unknown
    assert result.min_reduced_cost_bound == -math.inf
    assert not result.exact_stage

    path = make_path(5.0, [(0, 1.0)], -1.0)
    result = layer.PricingResult(
        columns=[path], status=layer.PricingStatus.Optimal, min_reduced_cost_bound=0.0
    )
    result.exact_stage = True
    result.columns = [path, path]
    assert result.columns[0].column.cost == 5.0 and len(result.columns) == 2
    assert result.exact_stage and result.min_reduced_cost_bound == 0.0


# MARK: Batches

GOOD_COLUMNS: dict[str, Any] = dict(
    ids=[0], cost=[1.0], lower=[0.0], upper=[1.0], starts=[0, 1], row_indices=[0], values=[1.0]
)


def test_a_column_batch() -> None:
    batch = layer.ColumnBatch(**GOOD_COLUMNS)
    assert len(batch) == 1
    assert batch.ids.dtype == np.uint64 and batch.starts.dtype == np.uint32
    assert batch.row_indices.dtype == np.uint32
    assert batch.cost.dtype == batch.lower.dtype == batch.upper.dtype == np.float64
    assert batch.values.dtype == np.float64
    assert batch.ids.tolist() == [0] and batch.cost.tolist() == [1.0]
    assert batch.lower.tolist() == [0.0] and batch.upper.tolist() == [1.0]
    assert batch.starts.tolist() == [0, 1] and batch.row_indices.tolist() == [0]
    assert batch.values.tolist() == [1.0]

    with pytest.raises(AttributeError):
        setattr(batch, "ids", np.zeros(1))  # a read-only property

    # Positional arguments, numpy inputs, no column at all.
    second = layer.ColumnBatch(
        np.array([7, 3]),
        np.array([1, 2]),
        [0, 0],
        [math.inf, 1],
        (0, 1, 3),
        [0, 0, 1],
        [1.0, 2.0, 3.0],
    )
    assert second.ids.tolist() == [7, 3] and second.cost.tolist() == [1.0, 2.0]
    assert second.upper.tolist() == [math.inf, 1.0]
    empty = layer.ColumnBatch([], [], [], [], [0], [], [])
    assert len(empty) == 0
    assert repr(second) == "ColumnBatch(2 columns, 3 entries)"
    assert repr(empty) == "ColumnBatch(0 columns, 0 entries)"


def test_the_arrays_of_a_batch_cannot_be_written() -> None:
    cost, lower, upper = np.array([1.0]), np.array([0.0]), np.array([1.0])
    columns = layer.ColumnBatch(
        np.array([0], dtype=np.uint64),
        cost,
        lower,
        upper,
        np.array([0, 1], dtype=np.uint32),
        np.array([0], dtype=np.uint32),
        np.array([1.0]),
    )
    rows = layer.RowBatch(lower, upper, ["a"])
    arrays = [columns.ids, columns.cost, columns.lower, columns.upper, columns.starts]
    arrays += [columns.row_indices, columns.values, rows.lower, rows.upper]

    for array in arrays:
        with pytest.raises(ValueError, match="read-only"):
            array[0] = 0

    with pytest.raises(TypeError):
        rows.names[0] = "b"  # type: ignore[index]  # pyright: ignore[reportIndexIssue]

    # The caller's own arrays stay writable, and a batch keeps what it was given: it holds copies.
    cost[0] = 2.0
    lower[0] = -1.0
    assert cost.flags.writeable and lower.flags.writeable
    assert columns.cost[0] == 1.0 and columns.lower[0] == 0.0 and rows.lower[0] == 0.0
    assert not np.shares_memory(columns.cost, cost)


def test_a_column_batch_is_checked() -> None:
    wrong_sizes: list[dict[str, Any]] = [{"cost": [1.0, 2.0]}, {"starts": [0]}, {"values": []}]

    for change in wrong_sizes:
        with pytest.raises(ValueError, match="one value per id"):
            layer.ColumnBatch(**(GOOD_COLUMNS | change))

    for starts in ([1, 1], [0, 0], [0, 2]):
        with pytest.raises(ValueError, match="starts must rise"):
            layer.ColumnBatch(**(GOOD_COLUMNS | {"starts": starts}))

    for name in ("ids", "cost", "starts", "values"):
        with pytest.raises(ValueError, match=f"ColumnBatch.{name}: expected a one-dimensional"):
            wrong: dict[str, Any] = GOOD_COLUMNS | {name: [[1]]}
            layer.ColumnBatch(**wrong)

    with pytest.raises(ValueError, match="one-dimensional"):
        scalar: dict[str, Any] = GOOD_COLUMNS | {"lower": 0.0}
        layer.ColumnBatch(**scalar)

    with pytest.raises(TypeError):
        layer.ColumnBatch(ids=[0])  # type: ignore[call-arg]  # pyright: ignore[reportCallIssue]


def test_a_row_batch() -> None:
    rows = layer.RowBatch([0.0, 1.0], [2.0, 3.0])
    assert len(rows) == 2 and rows.names == ()
    assert rows.lower.tolist() == [0.0, 1.0] and rows.upper.tolist() == [2.0, 3.0]
    assert rows.lower.dtype == rows.upper.dtype == np.float64

    with pytest.raises(AttributeError):
        setattr(rows, "lower", np.zeros(2))  # a read-only property

    named = layer.RowBatch([1.0, -math.inf], upper=np.array([1, 2]), names=["a", "b"])
    assert named.names == ("a", "b") and named.upper.tolist() == [1.0, 2.0]
    assert len(layer.RowBatch([], [])) == 0
    assert repr(rows) == "RowBatch(2 rows)" and repr(named) == "RowBatch(2 rows, named)"

    with pytest.raises(ValueError, match="same size"):
        layer.RowBatch([0.0], [1.0, 2.0])

    with pytest.raises(ValueError, match="same size"):
        layer.RowBatch([0.0], [1.0], ["a", "b"])

    with pytest.raises(ValueError, match="RowBatch.lower: expected a one-dimensional"):
        nested: Any = [[0.0]]
        layer.RowBatch(nested, [1.0])

    # A string is not a list of names, though Python iterates over its characters.
    with pytest.raises(TypeError):
        layer.RowBatch([0.0, 1.0], [2.0, 3.0], "ab")

    with pytest.raises(TypeError, match="RowBatch.names: expected strings"):
        layer.RowBatch([0.0, 1.0], [2.0, 3.0], [1, 2])  # type: ignore[list-item]


# MARK: The master solver


class Doubler(layer.MasterSolver):
    """A master solver written in Python: it implements the required methods, and adds
    rows."""

    def __init__(self) -> None:
        super().__init__()
        self.rows = 0

    def number_of_rows(self) -> int:
        return self.rows

    def row_bounds(self) -> tuple[FloatArray, FloatArray]:
        return np.zeros(self.rows), np.ones(self.rows)

    def add_rows(self, rows: layer.RowBatch) -> None:
        self.rows += len(rows)

    def add_columns(self, columns: layer.ColumnBatch) -> None:
        pass

    def solve_relaxation(
        self, params: layer.RelaxationParams = layer.RelaxationParams()
    ) -> layer.SolverStatus:
        return layer.SolverStatus.Optimal

    def objective(self) -> float:
        return 2.0

    def duals(self) -> FloatArray:
        return np.full(self.rows, 2.0)

    def primal(self, ids: IdArray) -> FloatArray:
        return np.full(len(ids), 2.0)

    def solve_integer(
        self, params: layer.IntegerParams = layer.IntegerParams()
    ) -> layer.SolverStatus:
        return layer.SolverStatus.Optimal

    def set_option(self, name: str, value: OptionValue) -> None:
        pass

    def capabilities(self) -> layer.Capabilities:
        return layer.Capabilities(add_rows=True)


class WithoutRows(Doubler):
    """A master solver that keeps the default of every optional method."""

    add_rows = layer.MasterSolver.add_rows


def test_a_class_missing_a_required_method_cannot_be_built() -> None:
    assert layer.MasterSolver.__abstractmethods__ == {
        "number_of_rows",
        "row_bounds",
        "add_columns",
        "solve_relaxation",
        "objective",
        "duals",
        "primal",
        "solve_integer",
        "set_option",
        "capabilities",
    }
    assert layer.Pricer.__abstractmethods__ == {"price"}

    class WithoutObjective(layer.MasterSolver):
        def number_of_rows(self) -> int:
            return 0

    with pytest.raises(TypeError, match="abstract"):
        WithoutObjective()  # type: ignore[abstract]  # pyright: ignore[reportAbstractUsage]

    with pytest.raises(TypeError, match="abstract"):
        layer.Pricer()  # type: ignore[abstract]  # pyright: ignore[reportAbstractUsage]


def test_the_optional_methods_have_defaults() -> None:
    solver = WithoutRows()

    with pytest.raises(NotImplementedError, match="^MasterSolver.add_rows: this master solver"):
        solver.add_rows(layer.RowBatch([0.0], [1.0]))

    assert solver.farkas() is None
    assert solver.dual_tolerance() == 0.0
    assert solver.largest_user_cost() == 0.0
    solver.restore_user_model()


def test_a_python_master_solver() -> None:
    solver = Doubler()
    solver.add_rows(layer.RowBatch([0.0, 0.0], [1.0, 1.0]))
    assert solver.number_of_rows() == 2 and solver.capabilities().add_rows
    assert solver.solve_relaxation() == layer.SolverStatus.Optimal
    assert solver.objective() == 2.0 and solver.duals().tolist() == [2.0, 2.0]


class Pricing(layer.Pricer):
    """A pricer written in Python."""

    def price(
        self,
        duals: Any,
        request: layer.PricingRequest = layer.PricingRequest(),
    ) -> layer.PricingResult:
        return layer.PricingResult(
            columns=[make_path(5.0, [(0, 1.0)], -1.0)],
            status=layer.PricingStatus.Optimal,
            exact_stage=request.reduced_cost_threshold < 0.0,
        )

    def cost_scale(self) -> float:
        return 3.0


class OnlyPrice(layer.Pricer):
    """A pricer that keeps the default of every optional method."""

    def price(
        self, duals: Any, request: layer.PricingRequest = layer.PricingRequest()
    ) -> layer.PricingResult:
        return layer.PricingResult()


def test_a_python_pricer() -> None:
    pricer = Pricing()
    result = pricer.price(np.array([1.0]), layer.PricingRequest(reduced_cost_threshold=-1.0))
    assert result.exact_stage and result.columns[0].column.cost == 5.0
    assert pricer.cost_scale() == 3.0

    # The optional methods' defaults.
    default = OnlyPrice()
    assert default.cost_scale() == 1.0
    default.update(layer.PricingChanges(costs=True))


# MARK: Importing


def test_importing_the_layer_loads_no_lp_solver_package() -> None:
    # A new process: this one may have loaded them already.
    code = (
        "import sys, rcspp.column_generation as layer;"
        "layer.HighspyMasterSolver;"
        "print('gurobipy' in sys.modules, 'highspy' in sys.modules)"
    )
    loaded = subprocess.run(
        [sys.executable, "-c", code],
        capture_output=True,
        text=True,
        check=True,
        env={**os.environ, "PYTHONPATH": os.pathsep.join(sys.path)},
    )
    assert loaded.stdout.strip() == "False False"


def test_a_master_solver_without_its_package_names_the_extra(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setitem(sys.modules, "highspy", None)  # as if it were not installed

    with pytest.raises(ImportError, match=r"pip install rcspp\[highspy\]"):
        layer.HighspyMasterSolver(object())  # type: ignore[arg-type]


def test_the_package_imports_nothing_of_the_example() -> None:
    root = os.path.dirname(layer.__file__)
    checked = 0

    for directory, _, files in os.walk(root):
        for file in files:
            if not file.endswith(".py"):
                continue

            with open(os.path.join(directory, file), encoding="utf-8") as source:
                tree = ast.parse(source.read())

            for node in ast.walk(tree):
                imported: list[str] = []

                if isinstance(node, ast.ImportFrom):
                    imported = [node.module or ""] + [alias.name for alias in node.names]
                elif isinstance(node, ast.Import):
                    imported = [alias.name for alias in node.names]

                for module in imported:
                    assert "cpp_column_generation" not in module.split("."), (file, module)
                    checked += 1

    assert checked > 0
