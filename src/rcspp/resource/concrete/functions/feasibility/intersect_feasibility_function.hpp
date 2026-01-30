// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <set>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

// Deduce the container/value type by calling get_value() on the concrete Resource
template <typename ResourceType,
          typename ValueType =
              std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>>
class IntersectFeasibilityFunction
    : public Clonable<IntersectFeasibilityFunction<ResourceType, ValueType>,
                      FeasibilityFunction<ResourceType>> {
    public:
        explicit IntersectFeasibilityFunction(
            const std::map<size_t, std::set<ValueType>>* values_by_node_id, bool forbidden = true)
            : values_by_node_id_(values_by_node_id), forbidden_(forbidden) {}

        auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            if (empty_) {
                return true;  // no values to check, always feasible
            }
            // if forbidden, return true if no intersection
            // if required (forbidden_ = false), return true if intersection
            return resource.intersects(values_.get_value()) ^ forbidden_;
        }

    private:
        const std::map<size_t, std::set<ValueType>>* const values_by_node_id_;
        ResourceType values_;
        bool forbidden_;  // values are forbidden or required
        bool empty_;

        void preprocess(size_t node_id) override {
            auto it = values_by_node_id_->find(node_id);
            if (it != values_by_node_id_->end()) {
                values_.set_value(it->second);
                empty_ = it->second.empty();
            } else {
                empty_ = true;
            }
        }
};
}  // namespace rcspp
