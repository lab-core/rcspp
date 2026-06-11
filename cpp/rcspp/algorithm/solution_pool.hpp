// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <limits>
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rcspp/algorithm/solution.hpp"

namespace rcspp {

// Activity stats for a column — stored in the pool and updated on every price() call
// and on update_activity() calls (LP basis membership).
struct ColumnActivity {
        size_t age = 0;        // price() rounds since last returned (reset on return/basis/re-add)
        size_t use_count = 0;  // times returned by price() (rc < threshold)
        size_t priced_count = 0;  // times this column was included in a price() call
        size_t created_at = 0;    // pool's pricing_count_ at insertion (diagnostic)
        bool last_was_negative = false;
        double last_reduced_cost = std::numeric_limits<double>::infinity();

        // Fraction of this column's own pricings in which it was returned: use_count /
        // priced_count. Per-column (independent of other views' pricing traffic) and always in [0,
        // 1]; a column that has never been priced returns 0.0.
        [[nodiscard]] double usage_rate() const {
            return priced_count == 0
                       ? 0.0
                       : static_cast<double>(use_count) / static_cast<double>(priced_count);
        }
};

// ─── Internal atomic activity storage ─────────────────────────────────────────
// Used inside Entry so that price() and update_activity() can hold shared_lock
// while concurrently updating per-entry counters without data races.
// ColumnActivity (the public, non-atomic type) is obtained via snapshot().
struct AtomicColumnActivity {
        std::atomic<size_t> age{0};
        std::atomic<size_t> use_count{0};
        std::atomic<size_t> priced_count{0};
        size_t created_at{0};  // set once at insertion; no concurrent write
        std::atomic<bool> last_was_negative{false};
        std::atomic<double> last_reduced_cost{std::numeric_limits<double>::infinity()};

        AtomicColumnActivity() = default;
        // Atomic types are not copy/move-constructible by default; provide them
        // so std::list<Entry> can grow and Entry can be moved into the list.
        AtomicColumnActivity(const AtomicColumnActivity& o) noexcept
            : age(o.age.load()),
              use_count(o.use_count.load()),
              priced_count(o.priced_count.load()),
              created_at(o.created_at),
              last_was_negative(o.last_was_negative.load()),
              last_reduced_cost(o.last_reduced_cost.load()) {}
        AtomicColumnActivity(AtomicColumnActivity&& o) noexcept
            : AtomicColumnActivity(static_cast<const AtomicColumnActivity&>(o)) {}
        AtomicColumnActivity& operator=(const AtomicColumnActivity&) = delete;

        /// Return a non-atomic snapshot suitable for the public ColumnActivity API.
        [[nodiscard]] ColumnActivity snapshot() const {
            return {age.load(std::memory_order_relaxed),
                    use_count.load(std::memory_order_relaxed),
                    priced_count.load(std::memory_order_relaxed),
                    created_at,
                    last_was_negative.load(std::memory_order_relaxed),
                    last_reduced_cost.load(std::memory_order_relaxed)};
        }
};

// ─── Forward declaration ──────────────────────────────────────────────────────
class FilteredSolutionPool;

// ─── SolutionPool ─────────────────────────────────────────────────────────────

// Storage-only pool of columns. All operations are performed through
// FilteredSolutionPool objects created via new_filter().
//
// Design notes:
// - Entries are stored in a std::list; id_index_ maps ColumnId → list iterator for O(1)
//   removal without index invalidation.
// - FilteredSolutionPools register themselves on construction and unregister on destruction.
//   add_unlocked() and remove_if_locked() auto-propagate to all registered pools.
// - All pricing, removal, and read operations are delegated to FilteredSolutionPool.
class SolutionPool {
        friend class FilteredSolutionPool;

    public:
        using ColumnId = uint64_t;
        static constexpr ColumnId kNoId = 0;

        struct PricedColumn {
                ColumnId id;
                double reduced_cost;
                const Solution* solution;  // points into pool entry; valid until pool modification
        };

        using Predicate = std::function<bool(ColumnId, const Solution&, const ColumnActivity&)>;

        /// @brief Create a scoped, filtered view over this pool.
        ///
        /// The new @ref FilteredSolutionPool is registered for auto-propagation:
        /// every subsequent @ref add_unlocked() call is forwarded automatically.
        /// @p filter=nullptr accepts all existing and future entries.
        ///
        /// @param filter Optional predicate ``(const Solution&) -> bool``.
        ///               Entries that do not pass the predicate are excluded from
        ///               the view; new columns are tested on arrival.
        /// @return A new @ref FilteredSolutionPool registered with this pool.
        [[nodiscard]] FilteredSolutionPool new_filter(
            std::function<bool(const Solution&)> filter = nullptr);

        /// @brief Create a filtered view restricted by row/arc membership.
        ///
        /// Convenience overload — builds a predicate from the four constraint
        /// vectors and delegates to the single-argument @ref new_filter().
        ///
        /// @param compulsory_rows    Solution must cover all these row indices.
        /// @param forbidden_rows     Solution must not cover any of these.
        /// @param compulsory_arc_ids Path must traverse all these arc IDs.
        /// @param forbidden_arc_ids  Path must not traverse any of these.
        /// @return A new @ref FilteredSolutionPool.
        [[nodiscard]] FilteredSolutionPool new_filter(std::vector<size_t> compulsory_rows,
                                                      std::vector<size_t> forbidden_rows,
                                                      std::vector<size_t> compulsory_arc_ids,
                                                      std::vector<size_t> forbidden_arc_ids);

        /// @brief Build a standalone filter predicate from row/arc constraints.
        ///
        /// Returns a callable ``(const Solution&) -> bool`` that can be passed to
        /// @ref new_filter() or combined with other predicates.
        ///
        /// @param compulsory_rows    Solution must cover all of these row indices.
        /// @param forbidden_rows     Solution must not cover any of these.
        /// @param compulsory_arc_ids Path must traverse all of these arc IDs.
        /// @param forbidden_arc_ids  Path must not traverse any of these.
        /// @return A filter predicate suitable for @ref new_filter().
        [[nodiscard]] static std::function<bool(const Solution&)> make_filter(
            std::vector<size_t> compulsory_rows = {}, std::vector<size_t> forbidden_rows = {},
            std::vector<size_t> compulsory_arc_ids = {},
            std::vector<size_t> forbidden_arc_ids = {});

        /// @brief Copy the internal LP SoA arrays for bulk transfer to an external store.
        ///
        /// Returns copies of the CSR-format LP data that is maintained alongside
        /// every @ref Entry.  Use this to populate a cross-process
        /// ``SharedPricingPool`` without iterating ``column.rows`` in Python:
        ///
        /// @code
        ///   std::vector<double> costs, coefs;
        ///   std::vector<uint32_t> starts, indices;
        ///   pool.get_lp_data(costs, starts, indices, coefs);
        /// @endcode
        ///
        /// @param[out] out_col_costs   LP cost per column (CSR column 0).
        /// @param[out] out_row_starts  CSR row-pointer array (size = n_cols + 1).
        /// @param[out] out_row_indices Constraint indices for each non-zero.
        /// @param[out] out_row_coefs   Coefficient values for each non-zero.
        void get_lp_data(std::vector<double>& out_col_costs, std::vector<uint32_t>& out_row_starts,
                         std::vector<uint32_t>& out_row_indices,
                         std::vector<double>& out_row_coefs) const {
            std::shared_lock lock(mutex_);
            out_col_costs = lp_.col_costs;
            out_row_starts = lp_.row_starts;
            out_row_indices = lp_.row_indices;
            out_row_coefs = lp_.row_coefs;
        }

    private:
        // ── SoA LP store: contiguous arrays for cache-friendly pricing ─────────
        // Append-only. lp_index in Entry is a permanent handle into these arrays.
        // Removed entries leave their slots orphaned (not freed); slots are never
        // reused and are only accessed while their ColumnId is still live.
        struct LpStore {
                std::vector<double> col_costs;  // col_costs[i] = column LP cost
                std::vector<uint32_t>
                    row_starts;  // CSR: rows for col i are [row_starts[i], row_starts[i+1])
                std::vector<uint32_t> row_indices;  // constraint indices
                std::vector<double> row_coefs;      // matching coefficients

                [[nodiscard]] size_t size() const { return col_costs.size(); }

                // Append one column's LP data; return its lp_index.
                // row_starts has n+1 entries for n columns (standard CSR sentinel).
                uint32_t append(const Column& col) {
                    const auto idx = static_cast<uint32_t>(col_costs.size());
                    col_costs.push_back(col.cost);
                    // Push start offset for the new column's rows.
                    if (row_starts.empty()) {
                        row_starts.push_back(0);
                    }
                    for (const auto& row : col.rows) {
                        row_indices.push_back(static_cast<uint32_t>(row.index));
                        row_coefs.push_back(static_cast<double>(row.coefficient));
                    }
                    // Push end-of-column sentinel: row_starts[idx+1].
                    row_starts.push_back(static_cast<uint32_t>(row_indices.size()));
                    return idx;
                }
        };

        struct Entry {
                ColumnId id;
                Solution solution;
                AtomicColumnActivity activity;
                uint32_t lp_index{0};  // index into LpStore arrays
        };
        using EntryIter = std::list<Entry>::iterator;

        mutable std::shared_mutex mutex_;
        std::list<Entry> entries_;
        LpStore lp_;  // parallel SoA LP data; indexed by Entry::lp_index
        // hash → iterators into entries_ (for deduplication)
        std::unordered_map<uint64_t, std::vector<EntryIter>> hash_index_;
        // id → iterator into entries_ (O(1) access and removal)
        std::unordered_map<ColumnId, EntryIter> id_index_;
        // registered FilteredSolutionPools for auto-propagation
        std::vector<FilteredSolutionPool*> registered_pools_;
        ColumnId next_id_{1};
        std::atomic<size_t> pricing_count_{0};

        // caller: if non-null, this FilteredSolutionPool handles its own update directly and
        // should be skipped during propagation.
        ColumnId add_unlocked(const Solution& sol, FilteredSolutionPool* caller = nullptr);

        std::vector<ColumnId> remove_if_locked(const Predicate& pred);

        // Hard-delete a specific set of ids. Used by the snapshot-based global_remove_if so the
        // user predicate runs with no lock held. Tolerant of ids already gone / duplicated.
        std::vector<ColumnId> remove_ids_locked(const std::vector<ColumnId>& ids);

        static bool passes_row_filter(const Solution& sol,
                                      const std::vector<size_t>& compulsory_rows,
                                      const std::vector<size_t>& forbidden_rows) {
            if (compulsory_rows.empty() && forbidden_rows.empty()) {
                return true;
            }
            std::unordered_set<size_t> present;
            present.reserve(sol.column.rows.size());
            for (const auto& row : sol.column.rows) {
                present.insert(row.index);
            }
            return std::ranges::all_of(compulsory_rows,
                                       [&](size_t idx) { return present.contains(idx); }) &&
                   std::ranges::none_of(forbidden_rows,
                                        [&](size_t idx) { return present.contains(idx); });
        }

        static bool passes_arc_filter(const Solution& sol,
                                      const std::vector<size_t>& compulsory_arc_ids,
                                      const std::vector<size_t>& forbidden_arc_ids) {
            if (compulsory_arc_ids.empty() && forbidden_arc_ids.empty()) {
                return true;
            }
            std::unordered_set<size_t> arc_set(sol.path_arc_ids.begin(), sol.path_arc_ids.end());
            return std::ranges::all_of(compulsory_arc_ids,
                                       [&](size_t arc_id) { return arc_set.contains(arc_id); }) &&
                   std::ranges::none_of(forbidden_arc_ids,
                                        [&](size_t arc_id) { return arc_set.contains(arc_id); });
        }
};

// ─── FilteredSolutionPool ─────────────────────────────────────────────────────

// The primary interface for all pool operations — pricing, activity tracking,
// adding columns, and local/global removal.
//
// Internally keeps a std::vector<Entry*> (filtered_entries_) for cache-friendly O(n_filtered)
// iteration over the pricing hot path, and an unordered_map<ColumnId, size_t> (filtered_ids_)
// for O(1) membership test and O(1) swap-and-pop removal.
//
// Auto-propagation: registers with its pool on construction; pool.add_unlocked() and
// pool.remove_if_locked() propagate to all registered pools automatically.
//
// B&B usage:
//   auto fp = pool.new_filter(FilteredSolutionPool::make_filter({}, {}, {}, {10}));
//   // ... run CG using fp.price(), fp.add(), fp.update_activity() ...
//   // Backtrack: fp goes out of scope → pool unaffected, unregisters automatically.
//
// Chain filtering:
//   auto fp2 = fp.new_filter(pred);   // further narrows fp; entries must pass both filters
//
// Local vs global removal:
//   fp.remove_if(pred)         → removes from this view only (B&B scoped, backtrackable)
//   fp.global_remove_if(pred)  → hard deletes from the pool, propagates to all views
class FilteredSolutionPool {
    public:
        using ColumnId = SolutionPool::ColumnId;
        using PricedColumn = SolutionPool::PricedColumn;
        using Predicate = SolutionPool::Predicate;

        // Build a filtered view. filter=nullptr accepts all existing and future entries.
        // Snapshots existing entries and registers under one lock (atomically, so no concurrent
        // add is missed), then evaluates the filter OFF the lock via populate_off_lock so a
        // re-entrant user filter cannot deadlock. Register is done last in the locked block so a
        // throwing snapshot leaves nothing registered.
        explicit FilteredSolutionPool(SolutionPool& pool,
                                      std::function<bool(const Solution&)> filter = nullptr)
            : pool_(pool), filter_(std::move(filter)) {
            std::vector<std::pair<ColumnId, Solution>> snapshot;
            {
                std::unique_lock lock(pool_.mutex_);
                snapshot.reserve(pool_.entries_.size());
                for (const auto& entry : pool_.entries_) {
                    snapshot.emplace_back(entry.id, entry.solution);
                }
                pool_.registered_pools_.push_back(this);
            }
            populate_off_lock(std::move(snapshot));
        }

        // Unregisters from pool on destruction.
        ~FilteredSolutionPool() {
            if (!registered_) {
                return;
            }
            std::unique_lock lock(pool_.mutex_);
            auto& reg = pool_.registered_pools_;
            reg.erase(std::ranges::find(reg, this));
        }

        FilteredSolutionPool(const FilteredSolutionPool&) = delete;
        FilteredSolutionPool& operator=(const FilteredSolutionPool&) = delete;

        // Move: re-registers this in place of other in pool's registration list.
        FilteredSolutionPool(FilteredSolutionPool&& other) noexcept
            : pool_(other.pool_),
              filter_(std::move(other.filter_)),
              filtered_entries_(std::move(other.filtered_entries_)),
              filtered_ids_(std::move(other.filtered_ids_)),
              registered_(other.registered_) {
            other.registered_ = false;
            if (registered_) {
                std::unique_lock lock(pool_.mutex_);
                auto& reg = pool_.registered_pools_;
                auto it = std::ranges::find(reg, &other);
                if (it != reg.end()) {
                    *it = this;
                } else {
                    reg.push_back(this);
                }
            }
        }

        FilteredSolutionPool& operator=(FilteredSolutionPool&&) = delete;

        // ── Write operations ──────────────────────────────────────────────────

        /// @brief Add a column to the main pool and optionally to this view.
        ///
        /// The column is always inserted into the root @ref SolutionPool and
        /// propagated to all other registered @ref FilteredSolutionPool instances.
        /// It is added to @b this view only when @p check_filter is false or the
        /// column passes this view's filter predicate.
        ///
        /// Duplicate detection: if the same arc path has been added before the
        /// existing @ref ColumnId is returned and no new entry is created.
        ///
        /// @param sol          Column to add.
        /// @param check_filter When false, bypass the filter and add unconditionally.
        /// @return The @ref ColumnId assigned by the pool (stable across calls).
        ColumnId add(const Solution& sol, bool check_filter = true) {
            std::unique_lock lock(pool_.mutex_);
            const auto id = pool_.add_unlocked(sol, this);
            if (!filtered_ids_.contains(id) && (!check_filter || accepts(sol))) {
                auto entry_it = pool_.id_index_.find(id);
                if (entry_it != pool_.id_index_.end()) {
                    filtered_ids_.emplace(id, filtered_entries_.size());
                    filtered_entries_.push_back(&(*entry_it->second));
                }
            }
            return id;
        }

        /// @brief Batch-add multiple columns under a single lock acquisition.
        ///
        /// Equivalent to calling @ref add(const Solution&, bool) in a loop but
        /// more efficient — only one lock acquire/release.
        ///
        /// @param solutions    Columns to add.
        /// @param check_filter When false, bypass the filter for all columns.
        /// @return Vector of @ref ColumnId values, one per input solution.
        std::vector<ColumnId> add(const std::vector<Solution>& solutions,
                                  bool check_filter = true) {
            std::unique_lock lock(pool_.mutex_);
            std::vector<ColumnId> ids;
            ids.reserve(solutions.size());
            for (const auto& sol : solutions) {
                const auto id = pool_.add_unlocked(sol, this);
                if (!filtered_ids_.contains(id) && (!check_filter || accepts(sol))) {
                    auto entry_it = pool_.id_index_.find(id);
                    if (entry_it != pool_.id_index_.end()) {
                        filtered_ids_.emplace(id, filtered_entries_.size());
                        filtered_entries_.push_back(&(*entry_it->second));
                    }
                }
                ids.push_back(id);
            }
            return ids;
        }

        /// @brief Compute reduced costs and return columns with ``rc < threshold``.
        ///
        /// Iterates the filtered subset and for each column computes:
        /// ``rc = col.cost - Σ(duals[row.index] × row.coefficient)``
        ///
        /// Uses @c shared_lock — multiple @ref FilteredSolutionPool instances on
        /// different threads can call @ref price() simultaneously because all
        /// @ref ColumnActivity writes are atomic.
        ///
        /// @param duals     LP dual values indexed by constraint index.
        /// @param threshold Only columns with ``rc < threshold`` are returned (default 0.0).
        /// @return          Priced columns with their ids, reduced costs, and @c Solution pointers.
        ///                  The returned @c Solution* is valid until the next structural
        ///                  modification of the pool.
        [[nodiscard]] std::vector<PricedColumn> price(const std::vector<double>& duals,
                                                      double threshold = 0.0) {
            std::shared_lock lock(pool_.mutex_);
            ++pool_.pricing_count_;  // std::atomic<size_t>: safe under shared_lock
            return price_subset_locked(duals, threshold);
        }

        /// @brief Update per-column activity based on LP basis membership.
        ///
        /// Intended to be called once per LP solve, after the master problem
        /// returns the set of columns that entered the LP basis.
        ///
        /// For each column in @b this view:
        ///   - In @p basis_ids → @c age reset to 0, @c last_was_negative = true.
        ///   - Not in @p basis_ids → @c age incremented by 1.
        ///
        /// Columns outside this view are not touched.  Does @b not increment
        /// @c pricing_count (that counter tracks @ref price() calls only).
        ///
        /// Uses @c shared_lock — safe to call concurrently with @ref price().
        ///
        /// @param basis_ids ColumnIds of columns currently in the LP basis.
        void update_activity(const std::vector<ColumnId>& basis_ids) {
            std::shared_lock lock(pool_.mutex_);
            const std::unordered_set<ColumnId> basis_set(basis_ids.begin(), basis_ids.end());
            for (SolutionPool::Entry* entry_ptr : filtered_entries_) {
                auto& entry = *entry_ptr;
                if (basis_set.contains(entry.id)) {
                    entry.activity.age = 0;
                    entry.activity.last_was_negative = true;
                } else {
                    ++entry.activity.age;
                    entry.activity.last_was_negative = false;
                }
            }
        }

        // ── Local removes (this view only — supports B&B backtracking) ─────────

        /// @brief Remove entries from @b this view only (main pool is unaffected).
        ///
        /// Supports Branch-and-Bound: the removed columns remain in the pool and
        /// in other @ref FilteredSolutionPool views; letting the filtered pool go
        /// out of scope restores full access automatically.
        ///
        /// The predicate is evaluated on a snapshot with @b no lock held (the
        /// @c shared_mutex is non-recursive; holding it while calling user code
        /// would deadlock on re-entry).  Phases:
        ///   1. Snapshot under @c shared_lock.
        ///   2. Evaluate predicate unlocked.
        ///   3. Apply removals under @c unique_lock.
        ///
        /// @param pred Callable ``(ColumnId, const Solution&, const ColumnActivity&) -> bool``.
        ///             Return @c true to remove the entry from this view.
        /// @return ColumnIds of the removed entries.
        std::vector<ColumnId> remove_if(const Predicate& pred) {
            std::vector<std::tuple<ColumnId, Solution, ColumnActivity>> snapshot;
            {
                std::shared_lock lock(pool_.mutex_);
                snapshot.reserve(filtered_entries_.size());
                for (const SolutionPool::Entry* entry_ptr : filtered_entries_) {
                    snapshot.emplace_back(entry_ptr->id,
                                          entry_ptr->solution,
                                          entry_ptr->activity.snapshot());
                }
            }
            std::vector<ColumnId> selected;
            for (const auto& [id, sol, act] : snapshot) {
                if (pred(id, sol, act)) {
                    selected.push_back(id);
                }
            }
            std::unique_lock lock(pool_.mutex_);
            std::vector<ColumnId> removed;
            removed.reserve(selected.size());
            for (const ColumnId id : selected) {
                if (on_remove_unlocked(id)) {  // local-only erase; tolerant of already-gone ids
                    removed.push_back(id);
                }
            }
            return removed;
        }

        /// @brief Remove from this view all columns whose path uses @p arc_id.
        ///
        /// Local removal only — main pool and other views are unaffected.
        /// Useful in Branch-and-Bound when branching on arc inclusion/exclusion.
        ///
        /// @param arc_id Arc to exclude.
        /// @return ColumnIds of the removed entries.
        std::vector<ColumnId> remove_if_arc_present(size_t arc_id) {
            std::unique_lock lock(pool_.mutex_);
            return remove_if_local([arc_id](ColumnId, const Solution& sol, const ColumnActivity&) {
                return std::ranges::find(sol.path_arc_ids, arc_id) != sol.path_arc_ids.end();
            });
        }

        /// @brief Remove stale columns from this view (local; main pool unaffected).
        ///
        /// A column is removed if it satisfies either criterion:
        ///   - @c age > @p max_age  (not returned by @ref price() for too long), or
        ///   - @c priced_count > 0 and @c usage_rate() < @p min_usage_rate.
        ///
        /// A never-priced column (@c priced_count == 0) is immune to the usage-rate
        /// criterion so freshly added columns are not immediately evicted.
        ///
        /// @param max_age         Remove if column has not been priced-and-returned for
        ///                        more than this many @ref price() calls.
        /// @param min_usage_rate  Remove if the column's usage fraction falls below this
        ///                        value (ignored for never-priced columns).
        /// @return ColumnIds of the removed entries.
        std::vector<ColumnId> remove_stale(size_t max_age, double min_usage_rate = 0.0) {
            std::unique_lock lock(pool_.mutex_);  // exclusive: remove_if_local mutates this view
            return remove_if_local(
                [max_age, min_usage_rate](ColumnId, const Solution&, const ColumnActivity& act) {
                    return act.age > max_age ||
                           (act.priced_count > 0 && act.usage_rate() < min_usage_rate);
                });
        }

        // ── Global hard deletes (from pool, propagates to all views) ───────────

        /// @brief Hard-delete columns from the main pool; propagates to all views.
        ///
        /// Unlike @ref remove_if() this permanently removes entries from the root
        /// @ref SolutionPool and all registered @ref FilteredSolutionPool instances.
        ///
        /// The predicate is evaluated on a snapshot with @b no lock held (same
        /// rationale as @ref remove_if()), then deletions are applied under an
        /// exclusive lock.
        ///
        /// @param pred Callable ``(ColumnId, const Solution&, const ColumnActivity&) -> bool``.
        /// @return ColumnIds of the deleted entries.
        std::vector<ColumnId> global_remove_if(const Predicate& pred) {
            std::vector<std::tuple<ColumnId, Solution, ColumnActivity>> snapshot;
            {
                std::shared_lock lock(pool_.mutex_);
                snapshot.reserve(pool_.entries_.size());
                for (const SolutionPool::Entry& entry : pool_.entries_) {
                    snapshot.emplace_back(entry.id, entry.solution, entry.activity.snapshot());
                }
            }
            std::vector<ColumnId> selected;
            for (const auto& [id, sol, act] : snapshot) {
                if (pred(id, sol, act)) {
                    selected.push_back(id);
                }
            }
            std::unique_lock lock(pool_.mutex_);
            return pool_.remove_ids_locked(selected);
        }

        /// @brief Hard-delete all columns whose path traverses @p arc_id from the pool.
        ///
        /// Propagates to all registered @ref FilteredSolutionPool instances.
        ///
        /// @param arc_id Arc to exclude globally.
        /// @return ColumnIds of the deleted entries.
        std::vector<ColumnId> global_remove_if_arc_present(size_t arc_id) {
            std::unique_lock lock(pool_.mutex_);
            return pool_.remove_if_locked(
                [arc_id](ColumnId, const Solution& sol, const ColumnActivity&) {
                    return std::ranges::find(sol.path_arc_ids, arc_id) != sol.path_arc_ids.end();
                });
        }

        /// @brief Hard-delete stale columns from the pool (propagates to all views).
        ///
        /// Same eviction criterion as @ref remove_stale() but applies globally:
        /// entries are permanently removed from the root @ref SolutionPool.
        ///
        /// @param max_age        Remove if @c age > @p max_age.
        /// @param min_usage_rate Remove if @c usage_rate() < this (when @c priced_count > 0).
        /// @return ColumnIds of the deleted entries.
        std::vector<ColumnId> global_remove_stale(size_t max_age, double min_usage_rate = 0.0) {
            std::unique_lock lock(pool_.mutex_);
            return pool_.remove_if_locked(
                [max_age, min_usage_rate](ColumnId, const Solution&, const ColumnActivity& act) {
                    return act.age > max_age ||
                           (act.priced_count > 0 && act.usage_rate() < min_usage_rate);
                });
        }

        /// @brief Re-sort the filtered view by @c lp_index for cache-friendly pricing.
        ///
        /// The @ref price_subset_locked loop accesses the SoA LP arrays
        /// (@c col_costs, @c row_starts, @c row_coefs…) via @c Entry::lp_index.
        /// When @c filtered_entries_ is sorted by @c lp_index ascending, these
        /// accesses are sequential and the hardware prefetcher can work effectively.
        ///
        /// Called automatically at filter construction via @ref populate_off_lock.
        /// New entries appended by @ref on_add_unlocked() always have the highest
        /// @c lp_index (monotone), so they preserve the sort order.  Only
        /// swap-and-pop removals may disrupt it; call this method after
        /// @ref remove_stale() or @ref remove_if() when performance matters.
        void sort_by_lp_index() {
            std::unique_lock lock(pool_.mutex_);
            sort_by_lp_index_unlocked();
        }

        /// @brief Purge stale entry pointers left by pool-level removals.
        ///
        /// If a column is deleted from the root pool without going through the
        /// normal @ref on_remove_unlocked() propagation path (e.g. direct pool
        /// manipulation), this view may hold dangling @c Entry* pointers.
        /// @ref cleanup() scans @c filtered_entries_ and drops any entries whose
        /// @c ColumnId is no longer present in @c id_index_.
        void cleanup() {
            std::unique_lock lock(pool_.mutex_);  // exclusive: mutates this view's containers
            std::vector<ColumnId> stale;
            for (const SolutionPool::Entry* entry_ptr : filtered_entries_) {
                if (!pool_.id_index_.contains(entry_ptr->id)) {
                    stale.push_back(entry_ptr->id);
                }
            }
            for (ColumnId id : stale) {
                on_remove_unlocked(id);
            }
        }

        // ── Read operations ───────────────────────────────────────────────────

        /// @brief Fetch a column's @ref Solution by @ref ColumnId.
        ///
        /// @param id ColumnId returned by @ref add() or @ref price().
        /// @return A copy of the @ref Solution, or @c std::nullopt if @p id is
        ///         not in this filtered view (@ref kNoId, not present, or filtered out).
        [[nodiscard]] std::optional<Solution> get(ColumnId id) const {
            if (id == SolutionPool::kNoId) {
                return std::nullopt;
            }
            std::shared_lock lock(pool_.mutex_);
            auto it = filtered_ids_.find(id);
            if (it == filtered_ids_.end()) {
                return std::nullopt;
            }
            return filtered_entries_[it->second]->solution;
        }

        /// @brief Fetch a column's @ref ColumnActivity snapshot by @ref ColumnId.
        ///
        /// Returns an atomic snapshot of the activity counters (@c age, @c use_count,
        /// @c last_reduced_cost, etc.) at the instant of the call.
        ///
        /// @param id ColumnId returned by @ref add() or @ref price().
        /// @return A @ref ColumnActivity snapshot, or @c std::nullopt if not in view.
        [[nodiscard]] std::optional<ColumnActivity> get_activity(ColumnId id) const {
            if (id == SolutionPool::kNoId) {
                return std::nullopt;
            }
            std::shared_lock lock(pool_.mutex_);
            auto it = filtered_ids_.find(id);
            if (it == filtered_ids_.end()) {
                return std::nullopt;
            }
            return filtered_entries_[it->second]->activity.snapshot();
        }

        /// @brief Fetch id, solution, and activity in a single lock acquisition.
        ///
        /// More efficient than calling @ref get() and @ref get_activity() separately
        /// when all three fields are needed.
        ///
        /// @param id ColumnId returned by @ref add() or @ref price().
        /// @return Tuple ``(ColumnId, Solution, ColumnActivity)``, or @c std::nullopt
        ///         if @p id is not in this filtered view.
        [[nodiscard]] std::optional<std::tuple<ColumnId, Solution, ColumnActivity>> get_entry(
            ColumnId id) const {
            if (id == SolutionPool::kNoId) {
                return std::nullopt;
            }
            std::shared_lock lock(pool_.mutex_);
            auto it = filtered_ids_.find(id);
            if (it == filtered_ids_.end()) {
                return std::nullopt;
            }
            const auto& e = *filtered_entries_[it->second];
            return std::make_tuple(e.id, e.solution, e.activity.snapshot());
        }

        /// @brief Total number of @ref price() calls on the root pool since creation.
        ///
        /// Used with @ref ColumnActivity::created_at to compute per-column lifetime
        /// statistics.  All @ref FilteredSolutionPool instances sharing the same root
        /// pool see the same counter.
        [[nodiscard]] size_t pricing_count() const {
            std::shared_lock lock(pool_.mutex_);
            return pool_.pricing_count_;
        }

        /// @brief Number of columns currently in this filtered view.
        [[nodiscard]] size_t size() const {
            std::shared_lock lock(pool_.mutex_);
            return filtered_entries_.size();
        }

        /// @brief Return a snapshot of all ``(ColumnId, Solution, ColumnActivity)`` tuples.
        ///
        /// Acquires a @c shared_lock and copies all entries visible in this filtered
        /// view.  Useful for populating external data structures (e.g. the Python
        /// @c SharedPricingPool) or for inspecting the full column set at once.
        ///
        /// @return Vector of tuples ordered by position in @c filtered_entries_.
        [[nodiscard]] std::vector<std::tuple<ColumnId, Solution, ColumnActivity>> get_all() const {
            std::shared_lock lock(pool_.mutex_);
            std::vector<std::tuple<ColumnId, Solution, ColumnActivity>> result;
            result.reserve(filtered_entries_.size());
            for (const SolutionPool::Entry* entry_ptr : filtered_entries_) {
                const auto& entry = *entry_ptr;
                result.emplace_back(entry.id, entry.solution, entry.activity.snapshot());
            }
            return result;
        }

        // ── Filtering ─────────────────────────────────────────────────────────

        /// @brief Create a further-narrowed filtered view from this view.
        ///
        /// Entries in the new pool must pass @b both this view's existing filter
        /// @b and @p pred.  The combined filter is applied to the current entries
        /// off-lock (to avoid deadlock with re-entrant predicates), then the new
        /// pool is registered with the root pool for future propagation.
        ///
        /// @param pred Additional predicate, or @c nullptr for no extra restriction.
        /// @return A new @ref FilteredSolutionPool narrowing this view.
        [[nodiscard]] FilteredSolutionPool new_filter(
            std::function<bool(const Solution&)> pred = nullptr) const;

        /// @brief Create a further-narrowed view restricted by row/arc membership.
        ///
        /// Convenience overload — builds the filter predicate from the constraint
        /// vectors and delegates to the single-argument @ref new_filter().
        ///
        /// @param compulsory_rows    Column must cover all these row indices.
        /// @param forbidden_rows     Column must not cover any of these.
        /// @param compulsory_arc_ids Path must traverse all these arc IDs.
        /// @param forbidden_arc_ids  Path must not traverse any of these.
        /// @return A new @ref FilteredSolutionPool narrowing this view.
        [[nodiscard]] FilteredSolutionPool new_filter(std::vector<size_t> compulsory_rows,
                                                      std::vector<size_t> forbidden_rows,
                                                      std::vector<size_t> compulsory_arc_ids,
                                                      std::vector<size_t> forbidden_arc_ids) const {
            return new_filter(make_filter(std::move(compulsory_rows),
                                          std::move(forbidden_rows),
                                          std::move(compulsory_arc_ids),
                                          std::move(forbidden_arc_ids)));
        }

        /// @brief Narrow @b this view in-place by composing an additional predicate.
        ///
        /// Unlike @ref new_filter() (which creates a separate pool), this method
        /// @b mutates the current view: entries that no longer pass the combined
        /// filter are removed from @c filtered_entries_ immediately.
        ///
        /// The predicate is evaluated off-lock on a snapshot to avoid deadlock.
        ///
        /// @param pred Additional filter; composed AND-wise with the existing filter.
        void add_filter(std::function<bool(const Solution&)> pred) {
            if (!pred) {
                return;
            }
            // Narrow the filter, then prune entries that no longer pass — evaluating the (user)
            // filter OFF the lock on a snapshot so it cannot deadlock on re-entry. `f` is a
            // copy of the composed filter so the off-lock evaluation is safe even if another
            // thread reassigns filter_ via a concurrent add_filter.
            std::function<bool(const Solution&)> f;
            std::vector<std::pair<ColumnId, Solution>> snapshot;
            {
                std::unique_lock lock(pool_.mutex_);
                filter_ = compose_and(std::move(filter_), std::move(pred));
                f = filter_;
                snapshot.reserve(filtered_entries_.size());
                for (const SolutionPool::Entry* entry_ptr : filtered_entries_) {
                    snapshot.emplace_back(entry_ptr->id, entry_ptr->solution);
                }
            }
            std::vector<ColumnId> to_remove;
            for (const auto& [id, sol] : snapshot) {
                if (f && !f(sol)) {
                    to_remove.push_back(id);
                }
            }
            std::unique_lock lock(pool_.mutex_);
            for (const ColumnId id : to_remove) {
                on_remove_unlocked(id);
            }
        }

        /// @brief Narrow this view in-place using row/arc membership constraints.
        ///
        /// Convenience overload — builds the filter predicate from the constraint
        /// vectors and delegates to the single-argument @ref add_filter().
        ///
        /// @param compulsory_rows    Column must cover all these row indices.
        /// @param forbidden_rows     Column must not cover any of these.
        /// @param compulsory_arc_ids Path must traverse all these arc IDs.
        /// @param forbidden_arc_ids  Path must not traverse any of these.
        void add_filter(std::vector<size_t> compulsory_rows, std::vector<size_t> forbidden_rows,
                        std::vector<size_t> compulsory_arc_ids,
                        std::vector<size_t> forbidden_arc_ids) {
            add_filter(make_filter(std::move(compulsory_rows),
                                   std::move(forbidden_rows),
                                   std::move(compulsory_arc_ids),
                                   std::move(forbidden_arc_ids)));
        }

        /// @brief Build a standalone filter predicate from row/arc membership constraints.
        ///
        /// Returns a callable that can be passed to @ref new_filter() or composed
        /// manually.  Equivalent to @ref SolutionPool::make_filter().
        ///
        /// @param compulsory_rows    Column must cover all these row indices.
        /// @param forbidden_rows     Column must not cover any of these.
        /// @param compulsory_arc_ids Path must traverse all these arc IDs.
        /// @param forbidden_arc_ids  Path must not traverse any of these.
        /// @return A filter predicate ``(const Solution&) -> bool``.
        [[nodiscard]] static std::function<bool(const Solution&)> make_filter(
            std::vector<size_t> compulsory_rows = {}, std::vector<size_t> forbidden_rows = {},
            std::vector<size_t> compulsory_arc_ids = {},
            std::vector<size_t> forbidden_arc_ids = {}) {
            return [cr = std::move(compulsory_rows),
                    fr = std::move(forbidden_rows),
                    ca = std::move(compulsory_arc_ids),
                    fa = std::move(forbidden_arc_ids)](const Solution& sol) {
                return SolutionPool::passes_row_filter(sol, cr, fr) &&
                       SolutionPool::passes_arc_filter(sol, ca, fa);
            };
        }

        [[nodiscard]] SolutionPool& pool() { return pool_; }
        [[nodiscard]] const SolutionPool& pool() const { return pool_; }

    private:
        friend class SolutionPool;

        SolutionPool& pool_;
        std::function<bool(const Solution&)> filter_;

        // Contiguous vector of Entry* for cache-friendly O(n_filtered) pricing iteration.
        // Invariant: filtered_entries_[filtered_ids_[id]] == entry_ptr for all active entries.
        // Removal uses swap-and-pop for O(1) amortised cost.
        std::vector<SolutionPool::Entry*> filtered_entries_;
        // ColumnId → index into filtered_entries_ for O(1) membership test and removal.
        std::unordered_map<ColumnId, size_t> filtered_ids_;

        bool registered_{true};

        // Chain constructor: a view over `parent`'s current entries whose filter is
        // (parent.filter_ AND additional_pred). Composing the filter (reads parent.filter_),
        // snapshotting parent's entries, and registering happen under ONE lock; the combined
        // filter is then evaluated OFF the lock via populate_off_lock. Register is last in the
        // locked block for exception safety.
        FilteredSolutionPool(const FilteredSolutionPool& parent,
                             std::function<bool(const Solution&)> additional_pred)
            : pool_(parent.pool_) {
            std::vector<std::pair<ColumnId, Solution>> snapshot;
            {
                std::unique_lock lock(pool_.mutex_);
                filter_ = compose_and(parent.filter_, std::move(additional_pred));
                snapshot.reserve(parent.filtered_entries_.size());
                for (const SolutionPool::Entry* entry_ptr : parent.filtered_entries_) {
                    snapshot.emplace_back(entry_ptr->id, entry_ptr->solution);
                }
                pool_.registered_pools_.push_back(this);
            }
            populate_off_lock(std::move(snapshot));
        }

        // Compose two filter predicates into (a AND b); a null predicate means accept-all, so a
        // null operand is dropped. Shared by the chain constructor (new_filter) and add_filter.
        static std::function<bool(const Solution&)> compose_and(
            std::function<bool(const Solution&)> a, std::function<bool(const Solution&)> b) {
            if (a && b) {
                return [a = std::move(a), b = std::move(b)](const Solution& sol) {
                    return a(sol) && b(sol);
                };
            }
            return a ? std::move(a) : std::move(b);
        }

        [[nodiscard]] bool accepts(const Solution& sol) const { return !filter_ || filter_(sol); }

        // Called by pool when a new entry is added (pool unique_lock already held).
        void on_add_unlocked(ColumnId id, SolutionPool::Entry& entry) {
            if (accepts(entry.solution)) {
                filtered_ids_.emplace(id, filtered_entries_.size());
                filtered_entries_.push_back(&entry);
            }
        }

        // Drop `id` from THIS view if present (pool unique_lock already held). Returns whether it
        // was present. Uses swap-and-pop for O(1) removal without invalidating other indices.
        bool on_remove_unlocked(ColumnId id) {
            if (id == SolutionPool::kNoId) {
                return false;
            }
            auto it = filtered_ids_.find(id);
            if (it == filtered_ids_.end()) {
                return false;
            }
            const size_t pos = it->second;
            const size_t last = filtered_entries_.size() - 1;
            if (pos != last) {
                // Move the last entry into the gap.
                filtered_entries_[pos] = filtered_entries_[last];
                filtered_ids_[filtered_entries_[pos]->id] = pos;
            }
            filtered_entries_.pop_back();
            filtered_ids_.erase(it);
            return true;
        }

        // Constructor helper: evaluate filter_ on `snapshot` OFF the lock (a re-entrant user
        // filter must not run while the mutex is held), then add the accepted entries that are
        // still present and not already added via propagation, under the lock. `this` must already
        // be registered. If the (user) filter throws, deregister and rethrow so a half-built view
        // is never left dangling in registered_pools_. New entries added during the off-lock
        // window arrive via propagation (we registered before releasing the lock); the
        // `filtered_ids_.contains` guard makes the apply phase idempotent against that overlap.
        // Sort filtered_entries_ by lp_index ascending so the pricing loop
        // accesses the SoA arrays (col_costs, row_starts, row_coefs…) sequentially,
        // maximising cache-line reuse.  O(n log n) once; new entries appended by
        // on_add_unlocked always have the highest lp_index so they naturally preserve
        // the order.  Caller must hold the pool unique_lock.
        void sort_by_lp_index_unlocked() {
            std::ranges::sort(filtered_entries_,
                              [](const SolutionPool::Entry* a, const SolutionPool::Entry* b) {
                                  return a->lp_index < b->lp_index;
                              });
            for (size_t i = 0; i < filtered_entries_.size(); ++i) {
                filtered_ids_[filtered_entries_[i]->id] = i;
            }
        }

        void populate_off_lock(std::vector<std::pair<ColumnId, Solution>> snapshot) {
            try {
                std::vector<ColumnId> accepted;
                accepted.reserve(snapshot.size());
                for (const auto& [id, sol] : snapshot) {
                    if (!filter_ || filter_(sol)) {
                        accepted.push_back(id);
                    }
                }
                std::unique_lock lock(pool_.mutex_);
                for (const ColumnId id : accepted) {
                    if (filtered_ids_.contains(id)) {
                        continue;  // already added via propagation during off-lock evaluation
                    }
                    auto it = pool_.id_index_.find(id);
                    if (it != pool_.id_index_.end()) {  // still present
                        filtered_ids_.emplace(id, filtered_entries_.size());
                        filtered_entries_.push_back(&(*it->second));
                    }
                }
                // Sort by lp_index so the pricing loop accesses SoA arrays sequentially.
                sort_by_lp_index_unlocked();
            } catch (...) {
                std::unique_lock lock(pool_.mutex_);
                auto& reg = pool_.registered_pools_;
                if (auto it = std::ranges::find(reg, this); it != reg.end()) {
                    reg.erase(it);
                }
                registered_ = false;
                throw;
            }
        }

        // Local removal without acquiring the pool lock (caller must hold at least shared lock).
        // Collects matching entries first, then applies swap-and-pop for each.
        std::vector<ColumnId> remove_if_local(const Predicate& pred) {
            std::vector<ColumnId> to_remove;
            for (const SolutionPool::Entry* entry_ptr : filtered_entries_) {
                if (pred(entry_ptr->id, entry_ptr->solution, entry_ptr->activity.snapshot())) {
                    to_remove.push_back(entry_ptr->id);
                }
            }
            for (ColumnId id : to_remove) {
                on_remove_unlocked(id);
            }
            return to_remove;
        }

        [[nodiscard]] std::vector<PricedColumn> price_subset_locked(
            const std::vector<double>& duals, double threshold) {
            std::vector<PricedColumn> result;
            const auto n_duals = static_cast<uint32_t>(duals.size());
            const auto& lp = pool_.lp_;
            for (SolutionPool::Entry* entry_ptr : filtered_entries_) {
                auto& entry = *entry_ptr;
                // Walk the SoA CSR arrays — all contiguous, no pointer chasing.
                // Accumulate in long double to match Row::coefficient's original precision;
                // narrow to double once for the LP-facing threshold comparison.
                const uint32_t li = entry.lp_index;
                long double rc = lp.col_costs[li];
                // row_starts has n+1 entries, so row_starts[li+1] is always valid.
                for (uint32_t j = lp.row_starts[li]; j < lp.row_starts[li + 1]; ++j) {
                    if (lp.row_indices[j] < n_duals) {
                        rc -= static_cast<long double>(lp.row_coefs[j]) *
                              static_cast<long double>(duals[lp.row_indices[j]]);
                    }
                }
                const auto rc_d = static_cast<double>(rc);
                entry.activity.last_reduced_cost = rc_d;
                ++entry.activity.priced_count;
                if (rc_d < threshold) {
                    entry.activity.age = 0;
                    ++entry.activity.use_count;
                    entry.activity.last_was_negative = true;
                    result.push_back({entry.id, rc_d, &entry.solution});
                } else {
                    ++entry.activity.age;
                    entry.activity.last_was_negative = false;
                }
            }
            return result;
        }
};

// ─── SolutionPool out-of-line definitions (require FilteredSolutionPool complete) ──

inline SolutionPool::ColumnId SolutionPool::add_unlocked(const Solution& sol,
                                                         FilteredSolutionPool* caller) {
    const auto hash = sol.get_hash();
    auto hit = hash_index_.find(hash);
    if (hit != hash_index_.end()) {
        for (const auto& entry_it : hit->second) {
            if (entry_it->solution.path_arc_ids == sol.path_arc_ids) {
                // Same arc path ⇒ same column identity. Refresh the stored column/cost with the
                // re-proposed values (master coefficients may have changed) and reset age, since a
                // just-regenerated column is not stale. path_arc_ids — and thus the hash — is
                // unchanged, so hash_index_/id_index_ stay valid and the id is preserved.
                entry_it->solution.column = sol.column;
                entry_it->solution.cost = sol.cost;
                entry_it->activity.age = 0;
                // Also refresh the SoA LP data so price() and get() stay consistent.
                // The row *indices* are fixed by the arc path; only the cost and
                // coefficient *values* may change (e.g. after update_reduced_costs).
                const uint32_t li = entry_it->lp_index;
                lp_.col_costs[li] = sol.column.cost;
                // Update coefficient values in-place: build a lookup from the new column
                // and overwrite matching entries in the existing CSR rows.
                const uint32_t rstart = lp_.row_starts[li];
                const uint32_t rend = lp_.row_starts[li + 1];
                for (uint32_t j = rstart; j < rend; ++j) {
                    for (const auto& row : sol.column.rows) {
                        if (static_cast<uint32_t>(row.index) == lp_.row_indices[j]) {
                            lp_.row_coefs[j] = static_cast<double>(row.coefficient);
                            break;
                        }
                    }
                }
                return entry_it->id;
            }
        }
    }
    const ColumnId new_id = next_id_++;
    AtomicColumnActivity activity;
    activity.created_at = pricing_count_.load(std::memory_order_relaxed);
    const uint32_t lp_idx = lp_.append(sol.column);
    entries_.push_back({new_id, sol, std::move(activity), lp_idx});
    auto new_it = std::prev(entries_.end());
    id_index_.emplace(new_id, new_it);
    hash_index_[hash].push_back(new_it);
    for (auto* fp : registered_pools_) {
        if (fp != caller) {
            fp->on_add_unlocked(new_id, *new_it);
        }
    }
    return new_id;
}

inline std::vector<SolutionPool::ColumnId> SolutionPool::remove_if_locked(const Predicate& pred) {
    std::vector<ColumnId> removed_ids;
    auto it = entries_.begin();
    while (it != entries_.end()) {
        if (pred(it->id, it->solution, it->activity.snapshot())) {
            const ColumnId cid = it->id;
            const uint64_t hash = it->solution.get_hash();

            auto& bucket = hash_index_[hash];
            if (auto f = std::ranges::find(bucket, it); f != bucket.end()) {
                bucket.erase(f);  // guard: never erase(end()) if the iterator isn't in the bucket
            }
            if (bucket.empty()) {
                hash_index_.erase(hash);
            }
            id_index_.erase(cid);

            for (auto* fp : registered_pools_) {
                fp->on_remove_unlocked(cid);
            }

            removed_ids.push_back(cid);
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
    return removed_ids;
}

inline std::vector<SolutionPool::ColumnId> SolutionPool::remove_ids_locked(
    const std::vector<ColumnId>& ids) {
    std::vector<ColumnId> removed;
    removed.reserve(ids.size());
    for (const ColumnId cid : ids) {
        auto idx_it = id_index_.find(cid);
        if (idx_it == id_index_.end()) {
            continue;  // already gone (removed concurrently, or a duplicate id in `ids`)
        }
        const EntryIter entry_it = idx_it->second;
        const uint64_t hash = entry_it->solution.get_hash();

        if (auto bucket_it = hash_index_.find(hash); bucket_it != hash_index_.end()) {
            auto& bucket = bucket_it->second;
            if (auto f = std::ranges::find(bucket, entry_it); f != bucket.end()) {
                bucket.erase(f);
            }
            if (bucket.empty()) {
                hash_index_.erase(bucket_it);
            }
        }
        id_index_.erase(idx_it);

        for (auto* fp : registered_pools_) {
            fp->on_remove_unlocked(cid);
        }

        removed.push_back(cid);
        entries_.erase(entry_it);
    }
    return removed;
}

inline FilteredSolutionPool SolutionPool::new_filter(std::function<bool(const Solution&)> filter) {
    return FilteredSolutionPool(*this, std::move(filter));
}

inline FilteredSolutionPool SolutionPool::new_filter(std::vector<size_t> compulsory_rows,
                                                     std::vector<size_t> forbidden_rows,
                                                     std::vector<size_t> compulsory_arc_ids,
                                                     std::vector<size_t> forbidden_arc_ids) {
    return new_filter(make_filter(std::move(compulsory_rows),
                                  std::move(forbidden_rows),
                                  std::move(compulsory_arc_ids),
                                  std::move(forbidden_arc_ids)));
}

inline std::function<bool(const Solution&)> SolutionPool::make_filter(
    std::vector<size_t> compulsory_rows, std::vector<size_t> forbidden_rows,
    std::vector<size_t> compulsory_arc_ids, std::vector<size_t> forbidden_arc_ids) {
    return FilteredSolutionPool::make_filter(std::move(compulsory_rows),
                                             std::move(forbidden_rows),
                                             std::move(compulsory_arc_ids),
                                             std::move(forbidden_arc_ids));
}

inline FilteredSolutionPool FilteredSolutionPool::new_filter(
    std::function<bool(const Solution&)> pred) const {
    // The chain constructor composes (this->filter_ AND pred) under a single lock, so filter_ is
    // never read here without synchronization.
    return {*this, std::move(pred)};
}

}  // namespace rcspp
