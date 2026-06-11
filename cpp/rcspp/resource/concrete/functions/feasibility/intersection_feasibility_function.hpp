// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

// ValueType is the element type stored in the per-node forbidden/required sets and is
// fed to ContainerResourceType::set_value. The default matches the element type the
// resource advertises; override it to point IFF at an alternative set_value overload.
template <typename ContainerResourceType,
          typename ValueType = typename ContainerResourceType::ValueType>
class IntersectionFeasibilityFunction
    : public Clonable<IntersectionFeasibilityFunction<ContainerResourceType, ValueType>,
                      FeasibilityFunction<ContainerResourceType>> {
    public:
        explicit IntersectionFeasibilityFunction(
            std::map<size_t, std::set<ValueType>> values_by_node_id, bool forbidden = true)
            : values_by_node_id_(std::make_shared<const std::map<size_t, std::set<ValueType>>>(
                  std::move(values_by_node_id))),
              forbidden_(forbidden) {}

        auto is_feasible(const ContainerResourceType& resource) -> bool override {
            if (empty_) {
                return true;  // no values to check, always feasible
            }
            // if forbidden, return true if no intersection
            // if required (forbidden_ = false), return true if intersection
            return resource.intersects(values_.get_value()) ^ forbidden_;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::set<ValueType>>> values_by_node_id_;
        ContainerResourceType values_;
        bool forbidden_;     // values are forbidden or required
        bool empty_ = true;  // to avoid checking intersection if no values to check

        void preprocess(size_t node_id) override {
            if (values_by_node_id_ == nullptr) {
                return;
            }
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
