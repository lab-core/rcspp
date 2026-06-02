// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <functional>
#include <limits>
#include <list>
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
        size_t age = 0;           // price() rounds since last returned (reset on return/basis/re-add)
        size_t use_count = 0;     // times returned by price() (rc < threshold)
        size_t priced_count = 0;  // times this column was included in a price() call
        size_t created_at = 0;    // pool's pricing_count_ at insertion (diagnostic)
        bool last_was_negative = false;
        double last_reduced_cost = std::numeric_limits<double>::infinity();

        // Fraction of this column's own pricings in which it was returned: use_count / priced_count.
        // Per-column (independent of other views' pricing traffic) and always in [0, 1]; a column
        // that has never been priced returns 0.0.
        [[nodiscard]] double usage_rate() const {
            return priced_count == 0
                       ? 0.0
                       : static_cast<double>(use_count) / static_cast<double>(priced_count);
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

        // Create a FilteredSolutionPool registered for auto-propagation.
        // filter=nullptr accepts all existing and future entries.
        [[nodiscard]] FilteredSolutionPool new_filter(
            std::function<bool(const Solution&)> filter = nullptr);

        // Convenience: build filter from row/arc constraints, then new_filter().
        [[nodiscard]] FilteredSolutionPool new_filter(std::vector<size_t> compulsory_rows,
                                                      std::vector<size_t> forbidden_rows,
                                                      std::vector<size_t> compulsory_arc_ids,
                                                      std::vector<size_t> forbidden_arc_ids);

        // Build a filter predicate from row/arc constraints.
        [[nodiscard]] static std::function<bool(const Solution&)> make_filter(
            std::vector<size_t> compulsory_rows = {}, std::vector<size_t> forbidden_rows = {},
            std::vector<size_t> compulsory_arc_ids = {},
            std::vector<size_t> forbidden_arc_ids = {});

    private:
        struct Entry {
                ColumnId id;
                Solution solution;
                ColumnActivity activity;
        };
        using EntryIter = std::list<Entry>::iterator;

        mutable std::shared_mutex mutex_;
        std::list<Entry> entries_;
        // hash → iterators into entries_ (for deduplication)
        std::unordered_map<uint64_t, std::vector<EntryIter>> hash_index_;
        // id → iterator into entries_ (O(1) access and removal)
        std::unordered_map<ColumnId, EntryIter> id_index_;
        // registered FilteredSolutionPools for auto-propagation
        std::vector<FilteredSolutionPool*> registered_pools_;
        ColumnId next_id_{1};
        size_t pricing_count_{0};

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
// Internally keeps a std::list<Entry*> (filtered_entries_) for O(n_filtered) iteration
// with no hash lookup per element, and an unordered_map<ColumnId, list::iterator>
// (filtered_ids_) for O(1) membership test and removal.
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

        // Add to main pool (always); add to this view if check_filter is false or filter accepts.
        // Always returns the pool-assigned id, even if this view's filter rejects the entry.
        // Pool propagates to OTHER registered FilteredSolutionPools; this one updates itself.
        ColumnId add(const Solution& sol, bool check_filter = true) {
            std::unique_lock lock(pool_.mutex_);
            const auto id = pool_.add_unlocked(sol, this);
            if (!filtered_ids_.contains(id) && (!check_filter || accepts(sol))) {
                auto entry_it = pool_.id_index_.find(id);
                if (entry_it != pool_.id_index_.end()) {
                    filtered_entries_.push_back(&(*entry_it->second));
                    filtered_ids_.emplace(id, std::prev(filtered_entries_.end()));
                }
            }
            return id;
        }

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
                        filtered_entries_.push_back(&(*entry_it->second));
                        filtered_ids_.emplace(id, std::prev(filtered_entries_.end()));
                    }
                }
                ids.push_back(id);
            }
            return ids;
        }

        // Price the filtered subset. Increments pricing_count; updates ColumnActivity
        // only for entries in this view.
        [[nodiscard]] std::vector<PricedColumn> price(const std::vector<double>& duals,
                                                      double threshold = 0.0) {
            std::unique_lock lock(pool_.mutex_);
            ++pool_.pricing_count_;
            return price_subset_locked(duals, threshold);
        }

        // Update activity for entries in this view based on LP basis membership. Basis membership
        // is not a pricing event, so a basis column has its age reset (it is "in use") and
        // last_was_negative set, but use_count/priced_count are left untouched (those count
        // pricings only, so usage_rate stays a true fraction). Columns NOT in basis_ids: ++age.
        // Columns outside this view: untouched. Does NOT increment pricing_count.
        void update_activity(const std::vector<ColumnId>& basis_ids) {
            std::unique_lock lock(pool_.mutex_);
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

        // Remove entries from this view only (NOT from the main pool).
        // The user predicate may inspect or even mutate the pool, so it is evaluated on a
        // snapshot with NO lock held — a std::shared_mutex is non-recursive, so running the
        // predicate under the lock would deadlock on re-entry. Phases: snapshot under a
        // shared lock → evaluate unlocked → apply under a unique lock.
        std::vector<ColumnId> remove_if(const Predicate& pred) {
            std::vector<std::tuple<ColumnId, Solution, ColumnActivity>> snapshot;
            {
                std::shared_lock lock(pool_.mutex_);
                snapshot.reserve(filtered_entries_.size());
                for (const SolutionPool::Entry* entry_ptr : filtered_entries_) {
                    snapshot.emplace_back(entry_ptr->id, entry_ptr->solution, entry_ptr->activity);
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

        // Remove from this view all columns whose path traverses arc_id.
        // The predicate is internal (cannot re-enter the pool), so it is safe to run under the
        // lock; the lock is exclusive because remove_if_local mutates this view.
        std::vector<ColumnId> remove_if_arc_present(size_t arc_id) {
            std::unique_lock lock(pool_.mutex_);
            return remove_if_local([arc_id](ColumnId, const Solution& sol, const ColumnActivity&) {
                return std::ranges::find(sol.path_arc_ids, arc_id) != sol.path_arc_ids.end();
            });
        }

        // Remove from this view entries where age > max_age, or (once priced) usage_rate is below
        // min_usage_rate. A never-priced column (priced_count == 0) is never evicted by the usage
        // criterion, so a freshly added column is not dropped before it has been priced.
        std::vector<ColumnId> remove_stale(size_t max_age, double min_usage_rate = 0.0) {
            std::unique_lock lock(pool_.mutex_);  // exclusive: remove_if_local mutates this view
            return remove_if_local([max_age, min_usage_rate](ColumnId,
                                                             const Solution&,
                                                             const ColumnActivity& act) {
                return act.age > max_age ||
                       (act.priced_count > 0 && act.usage_rate() < min_usage_rate);
            });
        }

        // ── Global hard deletes (from pool, propagates to all views) ───────────

        // Hard delete from pool, propagates to all registered FilteredSolutionPools.
        // Like remove_if, the user predicate is evaluated on a snapshot with NO lock held, then
        // the still-present matches are deleted under an exclusive lock.
        std::vector<ColumnId> global_remove_if(const Predicate& pred) {
            std::vector<std::tuple<ColumnId, Solution, ColumnActivity>> snapshot;
            {
                std::shared_lock lock(pool_.mutex_);
                snapshot.reserve(pool_.entries_.size());
                for (const SolutionPool::Entry& entry : pool_.entries_) {
                    snapshot.emplace_back(entry.id, entry.solution, entry.activity);
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

        // Hard delete all columns whose path traverses arc_id.
        std::vector<ColumnId> global_remove_if_arc_present(size_t arc_id) {
            std::unique_lock lock(pool_.mutex_);
            return pool_.remove_if_locked(
                [arc_id](ColumnId, const Solution& sol, const ColumnActivity&) {
                    return std::ranges::find(sol.path_arc_ids, arc_id) != sol.path_arc_ids.end();
                });
        }

        // Hard delete stale columns from pool (same criterion as remove_stale).
        std::vector<ColumnId> global_remove_stale(size_t max_age, double min_usage_rate = 0.0) {
            std::unique_lock lock(pool_.mutex_);
            return pool_.remove_if_locked([max_age, min_usage_rate](ColumnId,
                                                                    const Solution&,
                                                                    const ColumnActivity& act) {
                return act.age > max_age ||
                       (act.priced_count > 0 && act.usage_rate() < min_usage_rate);
            });
        }

        // Purge stale references left by pool-level removals that bypassed propagation.
        void cleanup() {
            std::unique_lock lock(pool_.mutex_);  // exclusive: mutates this view's containers
            for (auto list_it = filtered_entries_.begin(); list_it != filtered_entries_.end();) {
                const ColumnId id = (*list_it)->id;
                if (!pool_.id_index_.contains(id)) {
                    filtered_ids_.erase(id);
                    list_it = filtered_entries_.erase(list_it);
                } else {
                    ++list_it;
                }
            }
        }

        // ── Read operations ───────────────────────────────────────────────────

        [[nodiscard]] std::optional<Solution> get(ColumnId id) const {
            if (id == SolutionPool::kNoId) {
                return std::nullopt;
            }
            std::shared_lock lock(pool_.mutex_);
            auto it = filtered_ids_.find(id);
            if (it == filtered_ids_.end()) {
                return std::nullopt;
            }
            return (*it->second)->solution;
        }

        [[nodiscard]] std::optional<ColumnActivity> get_activity(ColumnId id) const {
            if (id == SolutionPool::kNoId) {
                return std::nullopt;
            }
            std::shared_lock lock(pool_.mutex_);
            auto it = filtered_ids_.find(id);
            if (it == filtered_ids_.end()) {
                return std::nullopt;
            }
            return (*it->second)->activity;
        }

        // Returns (id, solution, activity) in a single lock-acquire; nullopt if not in this view.
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
            const auto& e = *(*it->second);
            return std::make_tuple(e.id, e.solution, e.activity);
        }

        [[nodiscard]] size_t pricing_count() const {
            std::shared_lock lock(pool_.mutex_);
            return pool_.pricing_count_;
        }

        [[nodiscard]] size_t size() const {
            std::shared_lock lock(pool_.mutex_);
            return filtered_entries_.size();
        }

        [[nodiscard]] std::vector<std::tuple<ColumnId, Solution, ColumnActivity>> get_all() const {
            std::shared_lock lock(pool_.mutex_);
            std::vector<std::tuple<ColumnId, Solution, ColumnActivity>> result;
            result.reserve(filtered_entries_.size());
            for (const SolutionPool::Entry* entry_ptr : filtered_entries_) {
                const auto& entry = *entry_ptr;
                result.emplace_back(entry.id, entry.solution, entry.activity);
            }
            return result;
        }

        // ── Filtering ─────────────────────────────────────────────────────────

        // Create a further-narrowed FilteredSolutionPool: entries must pass BOTH this filter
        // AND pred. The new pool starts from this pool's current entries and is registered
        // with the root pool for auto-propagation using the combined filter.
        [[nodiscard]] FilteredSolutionPool new_filter(
            std::function<bool(const Solution&)> pred = nullptr) const;

        // Convenience: build filter from row/arc constraints, then new_filter().
        [[nodiscard]] FilteredSolutionPool new_filter(std::vector<size_t> compulsory_rows,
                                                      std::vector<size_t> forbidden_rows,
                                                      std::vector<size_t> compulsory_arc_ids,
                                                      std::vector<size_t> forbidden_arc_ids) const {
            return new_filter(make_filter(std::move(compulsory_rows),
                                          std::move(forbidden_rows),
                                          std::move(compulsory_arc_ids),
                                          std::move(forbidden_arc_ids)));
        }

        // Narrow this view further: updates filter_ and removes entries that no longer pass it.
        // Unlike new_filter(), this mutates the current pool instead of creating a new one.
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

        // Convenience: build filter from row/arc constraints, then add_filter().
        void add_filter(std::vector<size_t> compulsory_rows, std::vector<size_t> forbidden_rows,
                        std::vector<size_t> compulsory_arc_ids,
                        std::vector<size_t> forbidden_arc_ids) {
            add_filter(make_filter(std::move(compulsory_rows),
                                   std::move(forbidden_rows),
                                   std::move(compulsory_arc_ids),
                                   std::move(forbidden_arc_ids)));
        }

        // Build a filter predicate from row/arc constraints.
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

        // Ordered list of Entry* for efficient O(n_filtered) iteration with no hash lookups.
        using FilteredEntryList = std::list<SolutionPool::Entry*>;
        FilteredEntryList filtered_entries_;
        // ColumnId → iterator into filtered_entries_ for O(1) membership test and removal.
        std::unordered_map<ColumnId, FilteredEntryList::iterator> filtered_ids_;

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
                filtered_entries_.push_back(&entry);
                filtered_ids_.emplace(id, std::prev(filtered_entries_.end()));
            }
        }

        // Drop `id` from THIS view if present (pool unique_lock already held). Returns whether it
        // was present. Used both for pool-driven propagation and for remove_if's apply phase.
        bool on_remove_unlocked(ColumnId id) {
            if (id == SolutionPool::kNoId) {
                return false;
            }
            auto it = filtered_ids_.find(id);
            if (it != filtered_ids_.end()) {
                filtered_entries_.erase(it->second);
                filtered_ids_.erase(it);
                return true;
            }
            return false;
        }

        // Constructor helper: evaluate filter_ on `snapshot` OFF the lock (a re-entrant user
        // filter must not run while the mutex is held), then add the accepted entries that are
        // still present and not already added via propagation, under the lock. `this` must already
        // be registered. If the (user) filter throws, deregister and rethrow so a half-built view
        // is never left dangling in registered_pools_. New entries added during the off-lock
        // window arrive via propagation (we registered before releasing the lock); the
        // `filtered_ids_.contains` guard makes the apply phase idempotent against that overlap.
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
                        filtered_entries_.push_back(&(*it->second));
                        filtered_ids_.emplace(id, std::prev(filtered_entries_.end()));
                    }
                }
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
        std::vector<ColumnId> remove_if_local(const Predicate& pred) {
            std::vector<ColumnId> removed;
            for (auto list_it = filtered_entries_.begin(); list_it != filtered_entries_.end();) {
                auto& entry = **list_it;
                if (pred(entry.id, entry.solution, entry.activity)) {
                    removed.push_back(entry.id);
                    filtered_ids_.erase(entry.id);
                    list_it = filtered_entries_.erase(list_it);
                } else {
                    ++list_it;
                }
            }
            return removed;
        }

        [[nodiscard]] std::vector<PricedColumn> price_subset_locked(
            const std::vector<double>& duals, double threshold) {
            std::vector<PricedColumn> result;
            const size_t n_duals = duals.size();
            for (SolutionPool::Entry* entry_ptr : filtered_entries_) {
                auto& entry = *entry_ptr;
                double rc = entry.solution.column.cost;
                for (const Row& row : entry.solution.column.rows) {
                    if (row.index < n_duals) {
                        rc -= static_cast<double>(row.coefficient) * duals[row.index];
                    }
                }
                entry.activity.last_reduced_cost = rc;
                ++entry.activity.priced_count;  // this column was priced this round
                if (rc < threshold) {
                    entry.activity.age = 0;
                    ++entry.activity.use_count;
                    entry.activity.last_was_negative = true;
                    result.push_back({entry.id, rc, &entry.solution});
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
                return entry_it->id;
            }
        }
    }
    const ColumnId new_id = next_id_++;
    ColumnActivity activity;
    activity.created_at = pricing_count_;
    entries_.push_back({new_id, sol, std::move(activity)});
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
        if (pred(it->id, it->solution, it->activity)) {
            const ColumnId cid = it->id;
            const uint64_t hash = it->solution.get_hash();

            auto& bucket = hash_index_[hash];
            bucket.erase(std::ranges::find(bucket, it));
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
