// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// graph_impl.hpp defines PYBIND11_USE_SMART_HOLDER_AS_DEFAULT before the pybind11 includes.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "rcspp/rcspp.hpp"

namespace py = pybind11;

using namespace rcspp;

void init_solution_pool(py::module_& m) {  // NOLINT(readability-function-cognitive-complexity)
    // ColumnActivity is stored per-entry in SolutionPool and updated on every price() call.
    // usage_rate(current_pricing_count) returns use_count / (current - created_at).
    py::class_<ColumnActivity>(m, "ColumnActivity")
        .def(py::init<>())
        .def_readwrite("age", &ColumnActivity::age)
        .def_readwrite("use_count", &ColumnActivity::use_count)
        .def_readwrite("created_at", &ColumnActivity::created_at)
        .def_readwrite("last_was_negative", &ColumnActivity::last_was_negative)
        .def_readwrite("last_reduced_cost", &ColumnActivity::last_reduced_cost)
        .def("usage_rate", &ColumnActivity::usage_rate, py::arg("current_pricing_count"));

    // PricedColumn: result of FilteredSolutionPool.price().
    // id             → stable ColumnId for use in per-master variable/activity maps
    // reduced_cost   → newly computed rc = column.cost - dot(duals, rows)
    // solution       → reference to pool entry data (valid until next pool modification)
    py::class_<SolutionPool::PricedColumn>(m, "PricedColumn")
        .def_readonly("id", &SolutionPool::PricedColumn::id)
        .def_readonly("reduced_cost", &SolutionPool::PricedColumn::reduced_cost)
        .def_property_readonly(
            "solution",
            [](const SolutionPool::PricedColumn& pc) -> const Solution& { return *pc.solution; },
            py::return_value_policy::reference);

    // SolutionPool: storage-only. All pricing/removal operations go through
    // FilteredSolutionPool objects created via new_filter().
    py::class_<SolutionPool>(m, "SolutionPool")
        .def(py::init<>())
        // new_filter(filter=None): create a FilteredSolutionPool registered for auto-propagation.
        //   pool.new_filter()                                         # no filter — all entries
        //   pool.new_filter(lambda sol: ...)                          # custom predicate
        //   pool.new_filter(SolutionPool.make_filter(forbidden_arc_ids=[10]))  # row/arc filter
        //   pool.new_filter(forbidden_arc_ids=[10, 11])               # row/arc shortcuts
        .def(
            "new_filter",
            [](SolutionPool& pool,
               std::optional<py::function>
                   filter_fn,
               std::vector<size_t>
                   compulsory_rows,
               std::vector<size_t>
                   forbidden_rows,
               std::vector<size_t>
                   compulsory_arc_ids,
               std::vector<size_t>
                   forbidden_arc_ids) -> FilteredSolutionPool* {
                std::function<bool(const Solution&)> combined;
                if (!compulsory_rows.empty() || !forbidden_rows.empty() ||
                    !compulsory_arc_ids.empty() || !forbidden_arc_ids.empty()) {
                    combined = SolutionPool::make_filter(std::move(compulsory_rows),
                                                         std::move(forbidden_rows),
                                                         std::move(compulsory_arc_ids),
                                                         std::move(forbidden_arc_ids));
                }
                if (filter_fn) {
                    auto fn = *filter_fn;
                    auto py_pred = [fn](const Solution& sol) -> bool {
                        py::gil_scoped_acquire gil;
                        return py::cast<bool>(fn(sol));
                    };
                    if (combined) {
                        auto base = combined;
                        combined = [base, py_pred](const Solution& sol) {
                            return base(sol) && py_pred(sol);
                        };
                    } else {
                        combined = py_pred;
                    }
                }
                return new FilteredSolutionPool(pool, std::move(combined));
            },
            py::arg("filter") = py::none(),
            py::arg("compulsory_rows") = std::vector<size_t>{},
            py::arg("forbidden_rows") = std::vector<size_t>{},
            py::arg("compulsory_arc_ids") = std::vector<size_t>{},
            py::arg("forbidden_arc_ids") = std::vector<size_t>{},
            py::keep_alive<0, 1>())  // returned FilteredSolutionPool keeps pool alive
        // make_filter(...): build a filter predicate from row/arc constraints.
        .def_static(
            "make_filter",
            [](std::vector<size_t> compulsory_rows,
               std::vector<size_t>
                   forbidden_rows,
               std::vector<size_t>
                   compulsory_arc_ids,
               std::vector<size_t>
                   forbidden_arc_ids) {
                return SolutionPool::make_filter(std::move(compulsory_rows),
                                                 std::move(forbidden_rows),
                                                 std::move(compulsory_arc_ids),
                                                 std::move(forbidden_arc_ids));
            },
            py::arg("compulsory_rows") = std::vector<size_t>{},
            py::arg("forbidden_rows") = std::vector<size_t>{},
            py::arg("compulsory_arc_ids") = std::vector<size_t>{},
            py::arg("forbidden_arc_ids") = std::vector<size_t>{})
        .def_readonly_static("NO_ID", &SolutionPool::kNoId);

    // ── FilteredSolutionPool ──────────────────────────────────────────────────
    // A scoped, filtered view over a SolutionPool for use in B&B column generation.
    //
    // Creation:
    //   fp = pool.new_filter()                        # no filter — all entries
    //   fp = pool.new_filter(lambda sol: ...)         # custom predicate
    //   fp = pool.new_filter(forbidden_arc_ids=[10])  # row/arc shortcut kwargs
    //   fp2 = fp.new_filter(forbidden_rows=[0])       # chain: further narrow fp
    //
    // In-place narrowing:
    //   fp.add_filter(forbidden_arc_ids=[10])         # mutate fp; removes non-passing entries
    //
    // Local vs global removal:
    //   fp.remove_if(pred)         → removes from this view only (B&B scoped, backtrackable)
    //   fp.global_remove_if(pred)  → hard deletes from the pool, propagates to all views
    //
    // On B&B backtrack: let fp go out of scope. The main pool is unaffected;
    // the FilteredSolutionPool unregisters itself automatically on destruction.

    py::class_<FilteredSolutionPool>(m, "FilteredSolutionPool")
        // make_filter(...): build a filter predicate from row/arc constraints.
        .def_static(
            "make_filter",
            [](std::vector<size_t> compulsory_rows,
               std::vector<size_t>
                   forbidden_rows,
               std::vector<size_t>
                   compulsory_arc_ids,
               std::vector<size_t>
                   forbidden_arc_ids) {
                return FilteredSolutionPool::make_filter(std::move(compulsory_rows),
                                                         std::move(forbidden_rows),
                                                         std::move(compulsory_arc_ids),
                                                         std::move(forbidden_arc_ids));
            },
            py::arg("compulsory_rows") = std::vector<size_t>{},
            py::arg("forbidden_rows") = std::vector<size_t>{},
            py::arg("compulsory_arc_ids") = std::vector<size_t>{},
            py::arg("forbidden_arc_ids") = std::vector<size_t>{})
        // new_filter: create a further-narrowed FilteredSolutionPool.
        // Entries must pass BOTH this filter AND the new predicate/row-arc constraints.
        //   fp.new_filter()                       # no additional filter
        //   fp.new_filter(lambda sol: ...)        # custom predicate
        //   fp.new_filter(forbidden_arc_ids=[10]) # row/arc shortcuts
        .def(
            "new_filter",
            [](const FilteredSolutionPool& fp,
               std::optional<py::function>
                   pred,
               std::vector<size_t>
                   compulsory_rows,
               std::vector<size_t>
                   forbidden_rows,
               std::vector<size_t>
                   compulsory_arc_ids,
               std::vector<size_t>
                   forbidden_arc_ids) -> FilteredSolutionPool* {
                std::function<bool(const Solution&)> combined;
                if (!compulsory_rows.empty() || !forbidden_rows.empty() ||
                    !compulsory_arc_ids.empty() || !forbidden_arc_ids.empty()) {
                    combined = FilteredSolutionPool::make_filter(std::move(compulsory_rows),
                                                                 std::move(forbidden_rows),
                                                                 std::move(compulsory_arc_ids),
                                                                 std::move(forbidden_arc_ids));
                }
                if (pred) {
                    auto fn = *pred;
                    auto py_pred = [fn](const Solution& sol) -> bool {
                        py::gil_scoped_acquire gil;
                        return py::cast<bool>(fn(sol));
                    };
                    if (combined) {
                        auto base = combined;
                        combined = [base, py_pred](const Solution& sol) {
                            return base(sol) && py_pred(sol);
                        };
                    } else {
                        combined = py_pred;
                    }
                }
                return new FilteredSolutionPool(fp.new_filter(std::move(combined)));
            },
            py::arg("filter") = py::none(),
            py::arg("compulsory_rows") = std::vector<size_t>{},
            py::arg("forbidden_rows") = std::vector<size_t>{},
            py::arg("compulsory_arc_ids") = std::vector<size_t>{},
            py::arg("forbidden_arc_ids") = std::vector<size_t>{},
            py::keep_alive<0, 1>())  // returned pool keeps self (and thus root pool) alive
        // add_filter: mutate this view to further narrow by predicate/row-arc constraints.
        // Removes entries that no longer pass the combined filter.
        //   fp.add_filter(lambda sol: ...)        # custom predicate
        //   fp.add_filter(forbidden_arc_ids=[10]) # row/arc shortcuts
        .def(
            "add_filter",
            [](FilteredSolutionPool& fp,
               std::optional<py::function>
                   pred,
               std::vector<size_t>
                   compulsory_rows,
               std::vector<size_t>
                   forbidden_rows,
               std::vector<size_t>
                   compulsory_arc_ids,
               std::vector<size_t>
                   forbidden_arc_ids) {
                std::function<bool(const Solution&)> combined;
                if (!compulsory_rows.empty() || !forbidden_rows.empty() ||
                    !compulsory_arc_ids.empty() || !forbidden_arc_ids.empty()) {
                    combined = FilteredSolutionPool::make_filter(std::move(compulsory_rows),
                                                                 std::move(forbidden_rows),
                                                                 std::move(compulsory_arc_ids),
                                                                 std::move(forbidden_arc_ids));
                }
                if (pred) {
                    auto fn = *pred;
                    auto py_pred = [fn](const Solution& sol) -> bool {
                        py::gil_scoped_acquire gil;
                        return py::cast<bool>(fn(sol));
                    };
                    if (combined) {
                        auto base = combined;
                        combined = [base, py_pred](const Solution& sol) {
                            return base(sol) && py_pred(sol);
                        };
                    } else {
                        combined = py_pred;
                    }
                }
                fp.add_filter(std::move(combined));
            },
            py::arg("filter") = py::none(),
            py::arg("compulsory_rows") = std::vector<size_t>{},
            py::arg("forbidden_rows") = std::vector<size_t>{},
            py::arg("compulsory_arc_ids") = std::vector<size_t>{},
            py::arg("forbidden_arc_ids") = std::vector<size_t>{})
        .def(
            "add",
            [](FilteredSolutionPool& fp, const Solution& sol, bool check_filter) {
                return fp.add(sol, check_filter);
            },
            py::arg("solution"),
            py::arg("check_filter") = true)
        .def(
            "add",
            [](FilteredSolutionPool& fp, const std::vector<Solution>& sols, bool check_filter) {
                return fp.add(sols, check_filter);
            },
            py::arg("solutions"),
            py::arg("check_filter") = true)
        // price() prices only the filtered subset; updates ColumnActivity for those entries.
        .def("price", &FilteredSolutionPool::price, py::arg("duals"), py::arg("threshold") = 0.0)
        .def("update_activity", &FilteredSolutionPool::update_activity, py::arg("basis_ids"))
        // Local removes (this view only, supports B&B backtracking):
        .def(
            "remove_if",
            [](FilteredSolutionPool& fp, py::function pred) {
                return fp.remove_if([&pred](SolutionPool::ColumnId cid,
                                            const Solution& sol,
                                            const ColumnActivity& act) {
                    return py::cast<bool>(pred(cid, sol, act));
                });
            },
            py::arg("pred"))
        .def("remove_if_arc_present",
             &FilteredSolutionPool::remove_if_arc_present,
             py::arg("arc_id"))
        .def("remove_stale",
             &FilteredSolutionPool::remove_stale,
             py::arg("max_age"),
             py::arg("min_usage_rate") = 0.0)
        // Global hard deletes (propagate to all registered views):
        .def(
            "global_remove_if",
            [](FilteredSolutionPool& fp, py::function pred) {
                return fp.global_remove_if([&pred](SolutionPool::ColumnId cid,
                                                   const Solution& sol,
                                                   const ColumnActivity& act) {
                    return py::cast<bool>(pred(cid, sol, act));
                });
            },
            py::arg("pred"))
        .def("global_remove_if_arc_present",
             &FilteredSolutionPool::global_remove_if_arc_present,
             py::arg("arc_id"))
        .def("global_remove_stale",
             &FilteredSolutionPool::global_remove_stale,
             py::arg("max_age"),
             py::arg("min_usage_rate") = 0.0)
        .def("cleanup", &FilteredSolutionPool::cleanup)
        .def("get", &FilteredSolutionPool::get, py::arg("id"))
        .def("get_activity", &FilteredSolutionPool::get_activity, py::arg("id"))
        .def("get_entry", &FilteredSolutionPool::get_entry, py::arg("id"))
        .def("pricing_count", &FilteredSolutionPool::pricing_count)
        .def("get_all", &FilteredSolutionPool::get_all)
        .def("__len__", &FilteredSolutionPool::size)
        .def("size", &FilteredSolutionPool::size)
        .def("pool",
             py::overload_cast<>(&FilteredSolutionPool::pool),
             py::return_value_policy::reference);
}
