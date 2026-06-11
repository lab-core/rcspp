// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// graph_impl.hpp defines PYBIND11_USE_SMART_HOLDER_AS_DEFAULT before the pybind11 includes.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "rcspp/rcspp.hpp"

namespace py = pybind11;

using namespace rcspp;

namespace {
// Python-facing priced column. The C++ SolutionPool::PricedColumn keeps a zero-copy
// `const Solution*` into the pool entry (valid only until the next pool modification); this OWNS a
// copy of the Solution, taken at price() time while the entry is alive. The copy is paid only at
// the Python boundary, so the result can never dangle after a later pool mutation while C++
// pricing stays allocation-free.
struct PyPricedColumn {
        SolutionPool::ColumnId id;
        double reduced_cost;
        Solution solution;
};
}  // namespace

void init_solution_pool(py::module_& m) {  // NOLINT(readability-function-cognitive-complexity)
    // ColumnActivity is stored per-entry in SolutionPool and updated on every price() call.
    // usage_rate() returns use_count / priced_count (∈ [0, 1]; 0.0 if the column was never priced).
    py::class_<ColumnActivity>(m, "ColumnActivity")
        .def(py::init<>())
        .def_readwrite("age", &ColumnActivity::age)
        .def_readwrite("use_count", &ColumnActivity::use_count)
        .def_readwrite("priced_count", &ColumnActivity::priced_count)
        .def_readwrite("created_at", &ColumnActivity::created_at)
        .def_readwrite("last_was_negative", &ColumnActivity::last_was_negative)
        .def_readwrite("last_reduced_cost", &ColumnActivity::last_reduced_cost)
        .def("usage_rate", &ColumnActivity::usage_rate);

    // PricedColumn: result of FilteredSolutionPool.price().
    // id             → stable ColumnId for use in per-master variable/activity maps
    // reduced_cost   → newly computed rc = column.cost - dot(duals, rows)
    // solution       → an owned copy of the column's Solution (taken at price() time while the
    //                  pool entry was alive), so it stays valid after later pool mutations.
    py::class_<PyPricedColumn>(m, "PricedColumn")
        .def_readonly("id", &PyPricedColumn::id)
        .def_readonly("reduced_cost", &PyPricedColumn::reduced_cost)
        .def_readonly("solution", &PyPricedColumn::solution);

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
                   forbidden_arc_ids,
               std::optional<double>
                   min_usage_rate,
               std::optional<size_t>
                   max_age,
               std::optional<double>
                   max_last_rc) -> FilteredSolutionPool* {
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
                auto* fp = new FilteredSolutionPool(pool, std::move(combined));
                if (min_usage_rate.has_value() || max_age.has_value() || max_last_rc.has_value()) {
                    fp->remove_if([=](SolutionPool::ColumnId,
                                      const Solution&,
                                      const ColumnActivity& act) -> bool {
                        // (aged/stale entries) not produced by the basic unit-test helpers
                        if (max_age.has_value() && act.age > *max_age) {
                            return true;
                        }
                        if (min_usage_rate.has_value() && act.priced_count > 0 &&
                            act.usage_rate() < *min_usage_rate) {
                            return true;
                        }
                        if (max_last_rc.has_value() && act.last_reduced_cost >= *max_last_rc) {
                            return true;
                        }
                        return false;
                    });
                }
                return fp;
            },
            py::arg("filter") = py::none(),
            py::arg("compulsory_rows") = std::vector<size_t>{},
            py::arg("forbidden_rows") = std::vector<size_t>{},
            py::arg("compulsory_arc_ids") = std::vector<size_t>{},
            py::arg("forbidden_arc_ids") = std::vector<size_t>{},
            py::arg("min_usage_rate") = py::none(),
            py::arg("max_age") = py::none(),
            py::arg("max_last_rc") = py::none(),
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
        .def_readonly_static("NO_ID", &SolutionPool::kNoId)
        // get_lp_arrays(): return the internal LP CSR data as four owned numpy arrays.
        // The arrays own their data (allocated by numpy); the temporary std::vectors
        // are only used to copy the data out and are safe to destroy afterwards.
        .def("get_lp_arrays", [](const SolutionPool& pool) {
            std::vector<double> col_costs;
            std::vector<uint32_t> row_starts, row_indices;
            std::vector<double> row_coefs;
            pool.get_lp_data(col_costs, row_starts, row_indices, row_coefs);

            // Allocate owned numpy arrays and copy the data in — avoids returning
            // views into the local vectors which would be dangling after the lambda.
            auto make_f64 = [](const std::vector<double>& v) {
                auto arr = py::array_t<double>(static_cast<py::ssize_t>(v.size()));
                std::copy(v.begin(), v.end(), arr.mutable_data());
                return arr;
            };
            auto make_u32 = [](const std::vector<uint32_t>& v) {
                auto arr = py::array_t<uint32_t>(static_cast<py::ssize_t>(v.size()));
                std::copy(v.begin(), v.end(), arr.mutable_data());
                return arr;
            };

            return py::make_tuple(make_f64(col_costs),
                                  make_u32(row_starts),
                                  make_u32(row_indices),
                                  make_f64(row_coefs));
        });

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
                   forbidden_arc_ids,
               std::optional<double>
                   min_usage_rate,
               std::optional<size_t>
                   max_age,
               std::optional<double>
                   max_last_rc) -> FilteredSolutionPool* {
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
                auto* child = new FilteredSolutionPool(fp.new_filter(std::move(combined)));
                if (min_usage_rate.has_value() || max_age.has_value() || max_last_rc.has_value()) {
                    child->remove_if([=](SolutionPool::ColumnId,
                                         const Solution&,
                                         const ColumnActivity& act) -> bool {
                        if (max_age.has_value() && act.age > *max_age) return true;
                        if (min_usage_rate.has_value() && act.priced_count > 0 &&
                            act.usage_rate() < *min_usage_rate)
                            return true;
                        if (max_last_rc.has_value() && act.last_reduced_cost >= *max_last_rc)
                            return true;
                        return false;
                    });
                }
                return child;
            },
            py::arg("filter") = py::none(),
            py::arg("compulsory_rows") = std::vector<size_t>{},
            py::arg("forbidden_rows") = std::vector<size_t>{},
            py::arg("compulsory_arc_ids") = std::vector<size_t>{},
            py::arg("forbidden_arc_ids") = std::vector<size_t>{},
            py::arg("min_usage_rate") = py::none(),
            py::arg("max_age") = py::none(),
            py::arg("max_last_rc") = py::none(),
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
        // The binding copies each returned column's Solution into a Python-owned PyPricedColumn at
        // price() time (entry still alive), so the result never dangles after later pool edits.
        .def(
            "price",
            [](FilteredSolutionPool& fp, const std::vector<double>& duals, double threshold) {
                const auto priced = fp.price(duals, threshold);
                std::vector<PyPricedColumn> out;
                out.reserve(priced.size());
                for (const auto& pc : priced) {
                    out.push_back(
                        {.id = pc.id, .reduced_cost = pc.reduced_cost, .solution = *pc.solution});
                }
                return out;
            },
            py::arg("duals"),
            py::arg("threshold") = 0.0)
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
        // sort_by_lp_index(): re-sort filtered_entries_ by lp_index so the pricing
        // loop accesses SoA arrays sequentially (cache-friendly).  Called automatically
        // at filter construction; call again after batch adds if needed.
        .def("sort_by_lp_index", &FilteredSolutionPool::sort_by_lp_index)
        .def("get", &FilteredSolutionPool::get, py::arg("id"))
        .def("get_activity", &FilteredSolutionPool::get_activity, py::arg("id"))
        .def("get_entry", &FilteredSolutionPool::get_entry, py::arg("id"))
        .def("pricing_count", &FilteredSolutionPool::pricing_count)
        .def("get_all", &FilteredSolutionPool::get_all)
        // get_column_ids(): return ColumnIds as a contiguous uint64 numpy array.
        // Faster than get_all() when only the ids are needed (no Solution copies).
        .def("get_column_ids",
             [](const FilteredSolutionPool& fp) {
                 auto entries = fp.get_all();
                 auto result = py::array_t<uint64_t>(static_cast<py::ssize_t>(entries.size()));
                 auto buf = result.mutable_unchecked<1>();
                 for (size_t i = 0; i < entries.size(); ++i) {
                     buf(static_cast<py::ssize_t>(i)) =
                         static_cast<uint64_t>(std::get<0>(entries[i]));
                 }
                 return result;
             })
        // price_numpy(): like price() but returns (ids: uint64[n], rcs: float64[n])
        // instead of a list of PricedColumn objects — zero Python-object allocations.
        .def(
            "price_numpy",
            [](FilteredSolutionPool& fp,
               py::array_t<double, py::array::c_style | py::array::forcecast>
                   duals,
               double threshold) {
                const std::vector<double> dv(duals.data(), duals.data() + duals.size());
                const auto priced = fp.price(dv, threshold);
                auto ids = py::array_t<uint64_t>(static_cast<py::ssize_t>(priced.size()));
                auto rcs = py::array_t<double>(static_cast<py::ssize_t>(priced.size()));
                auto id_buf = ids.mutable_unchecked<1>();
                auto rc_buf = rcs.mutable_unchecked<1>();
                for (size_t i = 0; i < priced.size(); ++i) {
                    id_buf(static_cast<py::ssize_t>(i)) = static_cast<uint64_t>(priced[i].id);
                    rc_buf(static_cast<py::ssize_t>(i)) = priced[i].reduced_cost;
                }
                return py::make_tuple(ids, rcs);
            },
            py::arg("duals"),
            py::arg("threshold") = 0.0)
        .def("__len__", &FilteredSolutionPool::size)
        .def("size", &FilteredSolutionPool::size)
        .def("pool",
             py::overload_cast<>(&FilteredSolutionPool::pool),
             py::return_value_policy::reference);
}
