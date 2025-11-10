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

// helper: dependent false for static_assert
template <typename>
struct always_false : std::false_type {};

// Generic container-backed resource. C should be a concrete container type
// such as std::set<T>, std::vector<T>, etc.
template <typename C>
class ContainerResource : public ResourceBase<ContainerResource<C>> {
    public:
        using ValueType = C::value_type;

        ContainerResource() = default;
        explicit ContainerResource(C container) : container_(std::move(container)) {}
        explicit ContainerResource(const C& container) : container_(container) {}

        [[nodiscard]] const C& get_value() const { return container_; }
        void set_value(C container) { container_ = std::move(container); }

        // Methods that must be specialized for concrete C. Using a dependent
        // static_assert gives a readable compile-time message if an unsupported
        // container is instantiated.
        void add(const ValueType& /*value*/) {
            static_assert(always_false<C>::value,
                          "ContainerResource<C>::add must be specialized for this C");
        }
        void add(const C& /*container*/) {
            static_assert(always_false<C>::value,
                          "ContainerResource<C>::add(container) must be specialized for this C");
        }
        void remove(const ValueType& /*value*/) {
            static_assert(always_false<C>::value,
                          "ContainerResource<C>::remove must be specialized for this C");
        }
        [[nodiscard]] bool contains(const ValueType& /*value*/) const {
            static_assert(always_false<C>::value,
                          "ContainerResource<C>::contains must be specialized for this C");
            return false;
        }

        [[nodiscard]] bool includes(const C& /*other*/) const {
            static_assert(always_false<C>::value,
                          "ContainerResource<C>::includes must be specialized for this C");
            return false;
        }
        [[nodiscard]] bool intersects(const C& /*other*/) const {
            static_assert(always_false<C>::value,
                          "ContainerResource<C>::intersects must be specialized for this C");
            return false;
        }
        [[nodiscard]] C get_union(const C& /*other*/) const {
            static_assert(always_false<C>::value,
                          "ContainerResource<C>::get_union must be specialized for this C");
            return C();
        }

        // Convenience wrappers that operate on another ContainerResource<C>
        [[nodiscard]] virtual bool includes(const ContainerResource<C>& other) const {
            return includes(other.container_);
        }
        [[nodiscard]] virtual bool intersects(const ContainerResource<C>& other) const {
            return intersects(other.container_);
        }
        [[nodiscard]] virtual C get_union(const ContainerResource<C>& other) const {
            return get_union(other.container_);
        }

        void reset() override { container_.clear(); }
        [[nodiscard]] virtual size_t size() const { return container_.size(); }

    protected:
        C container_;
};

// Partial specialization for std::set<T>
template <typename T>
class ContainerResource<std::set<T>> : public ResourceBase<ContainerResource<std::set<T>>> {
    public:
        using Container = std::set<T>;
        using ValueType = T;

        ContainerResource() = default;
        explicit ContainerResource(Container container) : container_(std::move(container)) {}
        explicit ContainerResource(const Container& container) : container_(container) {}

        [[nodiscard]] const Container& get_value() const { return container_; }
        void set_value(Container container) { container_ = std::move(container); }

        void add(const T& value) { container_.insert(value); }
        void add(const Container& c) { container_.insert(c.begin(), c.end()); }
        void remove(const T& value) { container_.erase(value); }

        [[nodiscard]] bool contains(const T& value) const {
            return container_.find(value) != container_.end();
        }

        [[nodiscard]] bool includes(const Container& other_set) const {
            if (other_set.size() > container_.size()) {
                return false;
            }
            return std::includes(container_.begin(),
                                 container_.end(),
                                 other_set.begin(),
                                 other_set.end());
        }

        [[nodiscard]] bool intersects(const Container& other_set) const {
            if (container_.empty() || other_set.empty()) {
                return false;
            }
            auto it1 = container_.begin();
            auto it2 = other_set.begin();
            const auto end1 = container_.end();
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

        [[nodiscard]] Container get_union(const Container& other_set) const {
            Container result;
            std::set_union(container_.begin(),
                           container_.end(),
                           other_set.begin(),
                           other_set.end(),
                           std::inserter(result, result.begin()));
            return result;
        }

        void reset() override { container_.clear(); }

    protected:
        Container container_;
};

// Alias for convenience
template <typename T>
using SetResource = ContainerResource<std::set<T>>;

// Proper bitset specialization: implement bitset semantics using word vector
template <>
class ContainerResource<std::vector<uint64_t>>
    : public ResourceBase<ContainerResource<std::vector<uint64_t>>> {
    public:
        using Container = std::vector<uint64_t>;
        using ValueType = size_t;

        ContainerResource() = default;
        explicit ContainerResource(size_t nb_bits) { ensure_size(nb_bits); }
        explicit ContainerResource(Container words) : bits_(std::move(words)) {}
        explicit ContainerResource(const std::set<size_t>& indices) {
            for (auto idx : indices) {
                add(idx);
            }
        }

        // Note: idx >> 6 is a bitwise right shift of idx by 6 bits — equivalent to integer division
        // by 64 (floor). In this bitset code it computes which 64-bit word (slot) contains bit
        // number idx. The companion idx & 63 computes idx % 64 (bit offset inside that word).
        void add(size_t idx) {
            ensure_size(idx + 1);
            bits_[idx >> 6] |= (1ULL << (idx & 63));  // NOLINT
        }

        void add(const Container& other_words) {
            // OR the other words into this bitset
            const size_t other_words_count = other_words.size();
            ensure_size(other_words_count * 64);  // NOLINT
            for (size_t i = 0; i < other_words_count; ++i) {
                bits_[i] |= other_words[i];
            }
        }

        void remove(size_t idx) {
            if (idx < 64 * bits_.size()) {                 // avoid out-of-bounds  NOLINT
                bits_[idx >> 6] &= ~(1ULL << (idx & 63));  // NOLINT
            }
        }

        [[nodiscard]] bool contains(size_t idx) const {
            if (idx >= 64 * bits_.size()) {  // avoid out-of-bounds  NOLINT
                return false;
            }
            return ((bits_[idx >> 6] >> (idx & 63)) & 1ULL) != 0ULL;  // NOLINT
        }

        [[nodiscard]] bool includes(const Container& other) const {
            const size_t words_this = bits_.size();
            const size_t words_other = other.size();
            for (size_t i = 0; i < words_other; ++i) {
                const uint64_t ow = other[i];
                const uint64_t tw = (i < words_this) ? bits_[i] : 0ULL;
                if ((ow & ~tw) != 0ULL) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool intersects(const Container& other) const {
            const size_t words = std::min(bits_.size(), other.size());
            for (size_t i = 0; i < words; ++i) {
                if ((bits_[i] & other[i]) != 0ULL) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] Container get_union(const Container& other) const {
            const size_t words_max = std::max(bits_.size(), other.size());
            Container out(words_max, 0ULL);
            for (size_t i = 0; i < words_max; ++i) {
                const uint64_t a = (i < bits_.size()) ? bits_[i] : 0ULL;
                const uint64_t b = (i < other.size()) ? other[i] : 0ULL;
                out[i] = a | b;
            }
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
            return bits_.size() * 64ULL;  // NOLINT
        }

        void reset() override { bits_.clear(); }

        [[nodiscard]] size_t size() const { return bits_.size(); }

        [[nodiscard]] const Container& words() const { return bits_; }

    private:
        Container bits_;

        void ensure_size(size_t requested_nb_bits) {
            const size_t new_words = (requested_nb_bits + 63) / 64;
            if (bits_.size() < new_words) {
                bits_.resize(new_words, 0ULL);
            }
        }
};

using BitsetResource = ContainerResource<std::vector<uint64_t>>;

}  // namespace rcspp
