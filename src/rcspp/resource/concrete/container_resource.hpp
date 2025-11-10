// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <set>
#include <utility>

#include "rcspp/resource/base/resource_base.hpp"

namespace rcspp {

// Generic container-backed resource. C should be a concrete container type
// such as std::set<T>, std::vector<T>, etc.
template <typename C>
class ContainerResource : public ResourceBase<ContainerResource<C>> {
    public:
        using ValueType = C::value_type;

        ContainerResource() = default;
        explicit ContainerResource(C container) : container_(std::move(container)) {}

        [[nodiscard]] const C& get_container() const { return container_; }
        void set_container(C container) { container_ = std::move(container); }

        // Domain operations to be implemented by concrete subclasses
        virtual void add(const ValueType& value) = 0;
        virtual void add(const C& container) = 0;
        virtual void remove(const ValueType& value) = 0;
        virtual bool contains(const ValueType& value) const = 0;

        // Set-like queries
        virtual bool includes(const ContainerResource<C>& other) const = 0;
        virtual bool intersects(const ContainerResource<C>& other) const = 0;

        void reset() override { container_.clear(); }

    protected:
        C container_;
};

// Set specialization that takes advantage of std::set properties
template <typename T>
class SetBaseResource : public ContainerResource<std::set<T>> {
    public:
        using Container = std::set<T>;
        using ValueType = T;

        void add(const T& value) override { this->container_.insert(value); }

        void remove(const T& value) override { this->container_.erase(value); }

        bool contains(const T& value) const override {
            return this->container_.find(value) != this->container_.end();
        }

        // includes: check if *this contains all elements of other (uses std::includes)
        bool includes(const ContainerResource<Container>& other) const override {
            const auto& other_set = other.get_container();
            return std::includes(this->container_.begin(),
                                 this->container_.end(),
                                 other_set.begin(),
                                 other_set.end());
        }

        // intersects: test whether the two sets share any element. Uses an ordered-merge
        // scan over both sorted sets and runs in O(n + m) time.
        bool intersects(const ContainerResource<Container>& other) const override {
            const auto& other_set = other.get_container();
            if (this->container_.empty() || other_set.empty()) {
                return false;
            }

            // Use ordered-merge intersection: iterate both sets in-order and
            // advance the iterator with the smaller element. This is O(n + m) and
            // avoids log-factor lookups.
            auto it1 = this->container_.begin();
            auto it2 = other_set.begin();
            const auto end1 = this->container_.end();
            const auto end2 = other_set.end();

            while (it1 != end1 && it2 != end2) {
                if (*it1 < *it2) {
                    ++it1;
                } else if (*it2 < *it1) {
                    ++it2;
                } else {
                    // equal
                    return true;
                }
            }
            return false;
        }
};

}  // namespace rcspp
