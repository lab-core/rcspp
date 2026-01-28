// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType>
class SizeFeasibilityFunction
    : public Clonable<SizeFeasibilityFunction<ResourceType>, FeasibilityFunction<ResourceType>> {
    public:
        SizeFeasibilityFunction(
            size_t default_min_size, size_t default_max_size,
            const std::map<size_t, std::pair<size_t, size_t>>* min_max_size_by_node_id = nullptr)
            : min_max_size_by_node_id_(min_max_size_by_node_id),
              min_size_(default_min_size),
              max_size_(default_max_size) {}

        explicit SizeFeasibilityFunction(
            const std::map<size_t, std::pair<size_t, size_t>>* min_max_by_node_id,
            size_t default_min_size = 0,
            size_t default_max_size = std::numeric_limits<size_t>::max() / 2)  // prevent overflow
            : min_max_size_by_node_id_(min_max_by_node_id),
              min_size_(default_min_size),
              max_size_(default_max_size) {}

        auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            const size_t size = resource.size();
            return size >= min_size_ && size <= max_size_;
        }

    private:
        const std::map<size_t, std::pair<size_t, size_t>>* const min_max_size_by_node_id_;

        size_t min_size_;
        size_t max_size_;

        void preprocess(size_t node_id) override {
            if (min_max_size_by_node_id_ == nullptr) {
                return;
            }
            auto it = min_max_size_by_node_id_->find(node_id);
            // if not found, keep previous min_/max_ values
            if (it != min_max_size_by_node_id_->end()) {
                min_size_ = it->second.first;
                max_size_ = it->second.second;
            }
        }
};
}  // namespace rcspp
