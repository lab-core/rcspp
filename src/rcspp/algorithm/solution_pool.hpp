// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <functional>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rcspp/algorithm/solution.hpp"

namespace rcspp {

// Activity tracking metadata for a pooled solution.
struct SolutionActivity {
        size_t age = 0;  // pricing iterations since last returned (reset when below threshold)
        size_t use_count = 0;            // total times returned by price()
        bool last_was_negative = false;  // was the last computed reduced cost below threshold?
        double last_reduced_cost = std::numeric_limits<double>::infinity();
};

// Stores a pool of solutions for use in column generation re-pricing.
// Each solution carries a Column with its base arc cost and constraint coefficients,
// enabling re-pricing with new dual values without needing the original duals.
//
// Deduplication: solutions with the same hash AND the same column.rows are considered
// duplicates. Hash collisions with different rows are kept as distinct entries.
class SolutionPool {
    public:
        using Predicate = std::function<bool(const Solution&, const SolutionActivity&)>;

        // Add a single solution; skips exact duplicates (same hash + same rows).
        // In case of hash collision with different rows, both solutions are kept.
        void add(const Solution& sol) {
            const auto hash = sol.get_hash();
            const auto it = hash_index_.find(hash);
            if (it != hash_index_.end()) {
                for (const size_t idx : it->second) {
                    if (rows_equal(entries_[idx].solution.column.rows, sol.column.rows)) {
                        return;  // exact duplicate
                    }
                }
                it->second.push_back(entries_.size());
            } else {
                hash_index_.emplace(hash, std::vector<size_t>{entries_.size()});
            }
            entries_.push_back({sol, {}});
        }

        // Add a batch of solutions.
        void add(const std::vector<Solution>& solutions) {
            entries_.reserve(entries_.size() + solutions.size());
            for (const auto& sol : solutions) {
                add(sol);
            }
        }

        // Price all pooled solutions with duals.
        // reduced_cost = column.cost - dot(duals, column.rows)
        // Returns solutions with reduced_cost < threshold. For each:
        //   - solution.cost is updated to the new reduced cost
        //   - activity.last_reduced_cost and activity.last_was_negative are updated
        //   - activity.age is reset to 0 and activity.use_count is incremented
        // All others have activity.age incremented.
        [[nodiscard]] std::vector<Solution> price(const std::vector<double>& duals,
                                                  double threshold = 0.0) {
            std::vector<Solution> result;
            const size_t n_duals = duals.size();
            for (auto& [solution, activity] : entries_) {
                double rc = solution.column.cost;
                for (const Row& row : solution.column.rows) {
                    if (row.index < n_duals) {
                        rc -= static_cast<double>(row.coefficient) * duals[row.index];
                    }
                }
                solution.cost = rc;
                activity.last_reduced_cost = rc;
                if (rc < threshold) {
                    activity.last_was_negative = true;
                    activity.age = 0;
                    ++activity.use_count;
                    result.push_back(solution);
                } else {
                    activity.last_was_negative = false;
                    ++activity.age;
                }
            }
            return result;
        }

        // Remove all solutions for which pred(solution, activity) returns true.
        // Returns the removed solutions.
        [[nodiscard]] std::vector<Solution> remove_if(const Predicate& pred) {
            std::vector<Entry> kept;
            std::vector<Solution> removed;
            kept.reserve(entries_.size());
            for (auto& entry : entries_) {
                if (pred(entry.solution, entry.activity)) {
                    removed.push_back(std::move(entry.solution));
                } else {
                    kept.push_back(std::move(entry));
                }
            }
            entries_ = std::move(kept);
            rebuild_index();
            return removed;
        }

        // Convenience: remove solutions where activity.age > max_age or column.cost > max_cost.
        // Returns the removed solutions.
        [[nodiscard]] std::vector<Solution> remove(
            size_t max_age = std::numeric_limits<size_t>::max(),
            double max_cost = std::numeric_limits<double>::infinity()) {
            return remove_if([max_age, max_cost](const Solution& sol, const SolutionActivity& act) {
                return act.age > max_age || sol.column.cost > max_cost;
            });
        }

        [[nodiscard]] std::vector<Solution> get_solutions() const {
            std::vector<Solution> result;
            result.reserve(entries_.size());
            for (const auto& [solution, activity] : entries_) {
                result.push_back(solution);
            }
            return result;
        }

        // Returns all (solution, activity) pairs for inspection or custom filtering.
        [[nodiscard]] std::vector<std::pair<Solution, SolutionActivity>> get_entries() const {
            std::vector<std::pair<Solution, SolutionActivity>> result;
            result.reserve(entries_.size());
            for (const auto& [solution, activity] : entries_) {
                result.emplace_back(solution, activity);
            }
            return result;
        }

        [[nodiscard]] size_t size() const { return entries_.size(); }

    private:
        struct Entry {
                Solution solution;
                SolutionActivity activity;
        };

        std::vector<Entry> entries_;
        std::unordered_map<uint64_t, std::vector<size_t>> hash_index_;

        static bool rows_equal(const std::vector<Row>& a, const std::vector<Row>& b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (size_t i = 0; i < a.size(); ++i) {
                if (a[i].index != b[i].index || a[i].coefficient != b[i].coefficient) {
                    return false;
                }
            }
            return true;
        }

        void rebuild_index() {
            hash_index_.clear();
            hash_index_.reserve(entries_.size());
            for (size_t i = 0; i < entries_.size(); ++i) {
                hash_index_[entries_[i].solution.get_hash()].push_back(i);
            }
        }
};

}  // namespace rcspp
