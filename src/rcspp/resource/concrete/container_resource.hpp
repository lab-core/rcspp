// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstdint>  // NOLINT
#include <iterator>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource_base.hpp"

namespace rcspp {

// Generic container-backed resource. C should be a concrete container type
// such as std::set<T>, std::vector<T>, etc.
// ValueT is the logical value type of the resource (e.g. for a bitset resource
// the container element_type is uint64_t but the logical value type is size_t).
template <typename Container, typename DerivedType, typename ValueType>
class ContainerResource : public ResourceBase<DerivedType> {
    public:
        ContainerResource() = default;
        explicit ContainerResource(Container container) : container_(std::move(container)) {}
        explicit ContainerResource(const Container& container) : container_(container) {}

        [[nodiscard]] const Container& get_value() const { return container_; }
        virtual void set_value(Container container) { container_ = std::move(container); }

        // Methods that must be specialized for concrete C.
        virtual void add(const ValueType& /*value*/) = 0;
        virtual void add(const Container& /*container*/) = 0;
        virtual void remove(const ValueType& /*value*/) = 0;
        [[nodiscard]] virtual bool contains(const ValueType& /*value*/) const = 0;

        [[nodiscard]] virtual bool includes(const Container& /*other*/) const = 0;
        [[nodiscard]] virtual bool intersects(const Container& /*other*/) const = 0;
        [[nodiscard]] virtual Container get_union(const Container& /*other*/) const = 0;
        [[nodiscard]] virtual Container get_intersection(const Container& /*other*/) const = 0;

        [[nodiscard]] virtual size_t size() const { return container_.size(); }

        void reset() override { this->container_.clear(); }

    protected:
        Container container_;
};

// Partial specialization for std::set<T>
template <typename T>
class SetResource : public ContainerResource<std::set<T>, SetResource<T>, T> {
    public:
        using Container = std::set<T>;
        using ValueType = T;
        using Derived = SetResource<T>;

        SetResource() = default;
        explicit SetResource(Container container)
            : ContainerResource<Container, Derived, ValueType>(std::move(container)) {}
        explicit SetResource(const Container& container)
            : ContainerResource<Container, Derived, ValueType>(container) {}

        void add(const ValueType& value) override { this->container_.insert(value); }
        void add(const Container& c) override { this->container_.insert(c.begin(), c.end()); }
        void remove(const ValueType& value) override { this->container_.erase(value); }

        [[nodiscard]] bool contains(const ValueType& value) const override {
            return this->container_.find(value) != this->container_.end();
        }

        [[nodiscard]] bool includes(const Container& other_set) const override {
            if (other_set.size() > this->container_.size()) {
                return false;
            }
            return std::includes(this->container_.begin(),
                                 this->container_.end(),
                                 other_set.begin(),
                                 other_set.end());
        }

        [[nodiscard]] bool intersects(const Container& other_set) const override {
            if (this->container_.empty() || other_set.empty()) {
                return false;
            }
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
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] Container get_union(const Container& other_set) const override {
            Container result;
            std::set_union(this->container_.begin(),
                           this->container_.end(),
                           other_set.begin(),
                           other_set.end(),
                           std::inserter(result, result.begin()));
            return result;
        }

        [[nodiscard]] Container get_intersection(const Container& other_set) const override {
            Container result;
            std::set_intersection(this->container_.begin(),
                                  this->container_.end(),
                                  other_set.begin(),
                                  other_set.end(),
                                  std::inserter(result, result.begin()));
            return result;
        }
};

// Proper bitset specialization: implement bitset semantics using word vector
template <typename T>
class BitsetResource : public ContainerResource<std::vector<uint64_t>, BitsetResource<T>, T> {
    public:
        using Container = std::vector<uint64_t>;
        using ValueType = T;
        using Derived = BitsetResource<T>;

        BitsetResource() = default;
        explicit BitsetResource(const std::set<ValueType>& indices) {
            for (auto idx : indices) {
                add(idx);
            }
        }

        // convenience setter from an index set
        // -> necessary for an initializer with a set (i.e. ResourceInitializerTypeTuple)
        void set_value(const std::set<ValueType>& indices) {
            this->container_.clear();
            for (auto idx : indices) {
                add(idx);
            }
        }

        void set_value(Container container) override {
            ContainerResource<std::vector<uint64_t>, BitsetResource<T>, T>::set_value(container);
        }

        // Note: idx >> 6 is a bitwise right shift of idx by 6 bits — equivalent to integer division
        // by 64 (floor). In this bitset code it computes which 64-bit word (slot) contains bit
        // number idx. The companion idx & 63 computes idx % 64 (bit offset inside that word).
        void add(const ValueType& idx) override {
            ensure_size(idx + 1);
            this->container_[idx >> 6] |= (1ULL << (idx & 63));  // NOLINT
        }

        void add(const Container& other_words) override {
            // OR the other words into this bitset
            const size_t other_words_count = other_words.size();
            ensure_size(other_words_count * 64);  // NOLINT
            for (size_t i = 0; i < other_words_count; ++i) {
                this->container_[i] |= other_words[i];
            }
        }

        void remove(const ValueType& idx) override {
            if (idx < 64 * this->container_.size()) {                 // avoid out-of-bounds  NOLINT
                this->container_[idx >> 6] &= ~(1ULL << (idx & 63));  // NOLINT
            }
        }

        [[nodiscard]] bool contains(const ValueType& idx) const override {
            if (idx >= 64 * this->container_.size()) {  // avoid out-of-bounds  NOLINT
                return false;
            }
            return ((this->container_[idx >> 6] >> (idx & 63)) & 1ULL) != 0ULL;  // NOLINT
        }

        [[nodiscard]] bool includes(const Container& other) const override {
            const size_t words_this = this->container_.size();
            const size_t words_other = other.size();
            for (size_t i = 0; i < words_other; ++i) {
                const uint64_t ow = other[i];
                const uint64_t tw = (i < words_this) ? this->container_[i] : 0ULL;
                if ((ow & ~tw) != 0ULL) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool intersects(const Container& other) const override {
            const size_t words = std::min(this->container_.size(), other.size());
            for (size_t i = 0; i < words; ++i) {
                if ((this->container_[i] & other[i]) != 0ULL) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] Container get_union(const Container& other) const override {
            const size_t words_max = std::max(this->container_.size(), other.size());
            Container out(words_max, 0ULL);
            for (size_t i = 0; i < words_max; ++i) {
                const uint64_t a = (i < this->container_.size()) ? this->container_[i] : 0ULL;
                const uint64_t b = (i < other.size()) ? other[i] : 0ULL;
                out[i] = a | b;
            }
            return out;
        }

        [[nodiscard]] Container get_intersection(const Container& other) const override {
            const size_t words_min = std::min(this->container_.size(), other.size());
            Container out(words_min, 0ULL);
            for (size_t i = 0; i < words_min; ++i) {
                out[i] = this->container_[i] & other[i];
            }
            // keep trailing zero words for intersection to avoid resizing issues
            // // remove trailing zero words
            // while (!out.empty() && out.back() == 0ULL) {
            //     out.pop_back();
            // }
            return out;
        }

        [[nodiscard]] static size_t compute_used_bits(const Container& bits) {
            for (size_t i = bits.size(); i > 0; --i) {
                const uint64_t w = bits[i - 1];
                if (w == 0ULL) {
                    continue;
                }
                const unsigned msb = 63U - static_cast<unsigned>(__builtin_clzll(w));
                return (i - 1) * 64 + static_cast<size_t>(msb) + 1;  // NOLINT
            }
            return 0;
        }

        [[nodiscard]] size_t compute_allocated_bits() const {
            return this->container_.size() * 64ULL;  // NOLINT
        }

        [[nodiscard]] const Container& words() const { return this->container_; }

    private:
        // storage is inherited from ContainerResource as `container_`.

        void ensure_size(ValueType requested_nb_bits) {
            const size_t new_words = (requested_nb_bits + 63) / 64;
            if (this->container_.size() < new_words) {
                this->container_.resize(new_words, 0ULL);
            }
        }
};

}  // namespace rcspp
