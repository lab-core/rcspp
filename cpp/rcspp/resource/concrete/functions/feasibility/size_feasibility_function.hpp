// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Feasibility function that enforces a [min_size, max_size] bound on a container
///        resource's element count, with optional per-node overrides.
///
/// At each graph node the active size window is either the global default or the
/// per-node override supplied at construction.  The resource is feasible when
/// `min_size_ <= resource.size() <= max_size_`.
///
/// @tparam ResourceType The resource type that exposes a `size()` member returning the
///         number of elements currently stored.
template <typename ResourceType>
class SizeFeasibilityFunction
    : public Clonable<SizeFeasibilityFunction<ResourceType>, FeasibilityFunction<ResourceType>> {
    public:
        /// @brief Constructs the function with default size bounds and optional per-node
        ///        overrides.
        ///
        /// @param default_min_size Default minimum number of elements required at every node
        ///        that has no specific override.
        /// @param default_max_size Default maximum number of elements allowed at every node
        ///        that has no specific override.
        /// @param min_max_size_by_node_id Map from node id to a `{min_size, max_size}` pair
        ///        that overrides the defaults for that node.  May be empty.
        SizeFeasibilityFunction(
            size_t default_min_size, size_t default_max_size,
            std::map<size_t, std::pair<size_t, size_t>> min_max_size_by_node_id = {})
            : min_max_size_by_node_id_(
                  min_max_size_by_node_id.empty()
                      ? nullptr
                      : std::make_shared<const std::map<size_t, std::pair<size_t, size_t>>>(
                            std::move(min_max_size_by_node_id))),
              default_min_size_(default_min_size),
              default_max_size_(default_max_size),
              min_size_(default_min_size),
              max_size_(default_max_size) {}

        /// @brief Constructs the function from a per-node size map with optional global
        ///        fallback bounds.
        ///
        /// Nodes not present in the map fall back to `[default_min_size, default_max_size]`.
        /// The upper default is capped at `numeric_limits<size_t>::max() / 2` to prevent
        /// overflow when callers add increments.
        ///
        /// @param min_max_size_by_node_id Map from node id to a `{min_size, max_size}` pair.
        /// @param default_min_size Default minimum size for nodes not in the map.
        /// @param default_max_size Default maximum size for nodes not in the map.
        explicit SizeFeasibilityFunction(
            std::map<size_t, std::pair<size_t, size_t>> min_max_size_by_node_id,
            size_t default_min_size = 0,
            size_t default_max_size = std::numeric_limits<size_t>::max() / 2)  // prevent overflow
            : min_max_size_by_node_id_(
                  std::make_shared<const std::map<size_t, std::pair<size_t, size_t>>>(
                      std::move(min_max_size_by_node_id))),
              default_min_size_(default_min_size),
              default_max_size_(default_max_size),
              min_size_(default_min_size),
              max_size_(default_max_size) {}

        /// @brief Checks that the resource's size lies within the active [min_size_, max_size_]
        ///        window.
        ///
        /// @param resource The resource whose size is evaluated.
        /// @return `true` if `min_size_ <= resource.size() <= max_size_`.
        auto is_feasible(const ResourceType& resource) -> bool override {
            const size_t size = resource.size();
            return size >= min_size_ && size <= max_size_;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::pair<size_t, size_t>>> min_max_size_by_node_id_;

        size_t default_min_size_;
        size_t default_max_size_;
        size_t min_size_;
        size_t max_size_;

        void preprocess(size_t node_id) override {
            if (min_max_size_by_node_id_ == nullptr) {
                return;
            }
            auto it = min_max_size_by_node_id_->find(node_id);
            if (it != min_max_size_by_node_id_->end()) {
                min_size_ = it->second.first;
                max_size_ = it->second.second;
            } else {
                min_size_ = default_min_size_;
                max_size_ = default_max_size_;
            }
        }
};
}  // namespace rcspp
