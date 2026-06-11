// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <bit>      // NOLINT
#include <cstdint>  // NOLINT
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource_type.hpp"

namespace rcspp {

/// @brief Generic container-backed resource base class using CRTP.
///
/// Provides a uniform interface for set-like resources used by the RCSPP labelling
/// algorithm. The concrete container type @p Container (e.g. `std::set<T>` or
/// `std::vector<uint64_t>`) stores the actual elements, while @p ValueType is the
/// logical element type exposed to callers (which may differ, as in the bitset case
/// where the container stores `uint64_t` words but the logical type is `size_t`).
///
/// Derived classes must implement the pure-virtual mutating and query operations.
///
/// @tparam Container    The underlying storage type (e.g. `std::set<T>`).
/// @tparam DerivedType  The concrete CRTP subclass.
/// @tparam ValueType    The logical element type used in the public API.
template <typename Container, typename DerivedType, typename ValueType>
class ContainerResource {
    public:
        /// @brief Default constructor — initialises an empty container.
        ContainerResource() = default;

        /// @brief Constructs a ContainerResource wrapping the given container.
        ///
        /// @param container Initial container value (moved into the resource).
        explicit ContainerResource(Container container) : container_(std::move(container)) {}

        /// @brief Returns a const reference to the underlying container.
        ///
        /// @return The stored container.
        [[nodiscard]] const Container& get_value() const { return container_; }

        /// @brief Replaces the underlying container with @p container.
        ///
        /// @param container New container value (moved in).
        virtual void set_value(Container container) { container_ = std::move(container); }

        /// @brief Inserts a single logical value into the container.
        ///
        /// @param value Element to add.
        virtual void add(const ValueType& /*value*/) = 0;

        /// @brief Merges all elements of @p container into this resource.
        ///
        /// @param container Elements to add.
        virtual void add(const Container& /*container*/) = 0;

        /// @brief Removes a single logical value from the container.
        ///
        /// @param value Element to remove.
        virtual void remove(const ValueType& /*value*/) = 0;

        /// @brief Returns true if the container holds the given logical value.
        ///
        /// @param value Element to test.
        /// @return True if @p value is present.
        [[nodiscard]] virtual bool contains(const ValueType& /*value*/) const = 0;

        /// @brief Returns true if this container is a superset of @p other.
        ///
        /// @param other Container to check inclusion against.
        /// @return True if every element of @p other is also in this container.
        [[nodiscard]] virtual bool includes(const Container& /*other*/) const = 0;

        /// @brief Returns true if this container shares at least one element with @p other.
        ///
        /// @param other Container to test intersection against.
        /// @return True if the containers are not disjoint.
        [[nodiscard]] virtual bool intersects(const Container& /*other*/) const = 0;

        /// @brief Returns the union of this container and @p other.
        ///
        /// @param other Second operand.
        /// @return A new container holding all elements from either operand.
        [[nodiscard]] virtual Container get_union(const Container& /*other*/) const = 0;

        /// @brief Returns the intersection of this container and @p other.
        ///
        /// @param other Second operand.
        /// @return A new container holding only elements present in both operands.
        [[nodiscard]] virtual Container get_intersection(const Container& /*other*/) const = 0;

        /// @brief Returns the set difference: elements in this container but not in @p other.
        ///
        /// @param other Elements to exclude.
        /// @return A new container with the elements of @p other removed.
        [[nodiscard]] virtual Container subtract(const Container& /*other*/) const = 0;

        /// @brief Returns the number of logical elements stored in the container.
        ///
        /// @return Element count.
        [[nodiscard]] virtual size_t size() const { return container_.size(); }

        /// @brief Returns true if the container holds no elements.
        ///
        /// @return True when the container is empty.
        [[nodiscard]] virtual bool empty() const { return container_.empty(); }

        /// @brief Removes all elements from the container.
        void reset() { this->container_.clear(); }

        /// @brief Returns a human-readable string representation of the container.
        ///
        /// @return String of the form `{e1,e2,...}`.
        [[nodiscard]] virtual std::string to_string() const { return to_string(container_); }

        /// @brief Returns a human-readable string for any iterable collection @p list.
        ///
        /// @tparam C Any iterable container whose elements support `operator<<`.
        /// @param list Collection to stringify.
        /// @return String of the form `{e1,e2,...}`.
        template <typename C>
        [[nodiscard]] std::string to_string(const C& list) const {
            std::ostringstream oss;
            oss << "{";
            bool first = true;
            for (auto v : list) {
                if (first) {
                    first = false;
                } else {
                    oss << ",";
                }
                oss << v;
            }
            oss << "}";
            return oss.str();
        }

    protected:
        /// @brief The underlying storage container.
        Container container_;
};

/// @brief ContainerResource specialisation backed by `std::set<T>`.
///
/// All set operations (union, intersection, difference, inclusion, membership)
/// are implemented with the standard ordered-set algorithms, giving O(n) complexity
/// for binary operations and O(log n) for single-element mutations.
///
/// @tparam T Element type stored in the set; must be LessThanComparable.
template <typename T>
class SetResource : public ContainerResource<std::set<T>, SetResource<T>, T> {
    public:
        /// @brief Alias for the underlying container type.
        using Container = std::set<T>;
        /// @brief Logical value type (same as the element type for sets).
        using ValueType = T;
        /// @brief CRTP derived type alias.
        using Derived = SetResource<T>;

        /// @brief Default constructor — creates an empty set resource.
        SetResource() = default;

        /// @brief Constructs a SetResource from an existing set.
        ///
        /// @param container Initial set (moved in).
        explicit SetResource(Container container)
            : ContainerResource<Container, Derived, ValueType>(std::move(container)) {}

        /// @brief Inserts @p value into the set.
        ///
        /// @param value Element to insert.
        void add(const ValueType& value) override { this->container_.insert(value); }

        /// @brief Inserts all elements from @p c into the set.
        ///
        /// @param c Elements to insert.
        void add(const Container& c) override { this->container_.insert(c.begin(), c.end()); }

        /// @brief Erases @p value from the set.
        ///
        /// @param value Element to remove.
        void remove(const ValueType& value) override { this->container_.erase(value); }

        /// @brief Returns true if @p value is a member of the set.
        ///
        /// @param value Element to test.
        /// @return True if @p value is present.
        [[nodiscard]] bool contains(const ValueType& value) const override {
            return this->container_.find(value) != this->container_.end();
        }

        /// @brief Returns true if this set is a superset of @p other_set.
        ///
        /// @param other_set Set whose elements must all be present in this set.
        /// @return True if every element of @p other_set is in this set.
        [[nodiscard]] bool includes(const Container& other_set) const override {
            if (other_set.size() > this->container_.size()) {
                return false;
            }
            return std::includes(this->container_.begin(),
                                 this->container_.end(),
                                 other_set.begin(),
                                 other_set.end());
        }

        /// @brief Returns true if this set and @p other_set share at least one element.
        ///
        /// @param other_set Set to test against.
        /// @return True if the two sets are not disjoint.
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

        /// @brief Returns the union of this set and @p other_set.
        ///
        /// @param other_set Second operand.
        /// @return A new set containing all elements from either set.
        [[nodiscard]] Container get_union(const Container& other_set) const override {
            Container result;
            std::set_union(this->container_.begin(),
                           this->container_.end(),
                           other_set.begin(),
                           other_set.end(),
                           std::inserter(result, result.begin()));
            return result;
        }

        /// @brief Returns the intersection of this set and @p other_set.
        ///
        /// @param other_set Second operand.
        /// @return A new set containing only elements present in both sets.
        [[nodiscard]] Container get_intersection(const Container& other_set) const override {
            Container result;
            std::set_intersection(this->container_.begin(),
                                  this->container_.end(),
                                  other_set.begin(),
                                  other_set.end(),
                                  std::inserter(result, result.begin()));
            return result;
        }

        /// @brief Returns the set difference: elements in this set but not in @p other_set.
        ///
        /// @param other_set Elements to exclude.
        /// @return A new set with the elements of @p other_set removed.
        [[nodiscard]] Container subtract(const Container& other_set) const override {
            Container result;
            std::set_difference(this->container_.begin(),
                                this->container_.end(),
                                other_set.begin(),
                                other_set.end(),
                                std::inserter(result, result.begin()));
            return result;
        }
};

/// @brief ContainerResource specialisation implementing bitset semantics over a word vector.
///
/// Elements are non-negative integer indices of type @p T.  Internally the bits are
/// packed into a dynamically-sized `std::vector<uint64_t>`, so each 64-bit word holds
/// 64 bits.  The word index for bit @p idx is `idx >> 6` (= `idx / 64`) and the bit
/// offset within that word is `idx & 63` (= `idx % 64`).
///
/// All binary set operations (union, intersection, difference) run in O(words) time,
/// which is significantly faster than the sorted-set approach for dense index ranges.
///
/// @tparam T Unsigned integer type used as the element (bit index) type.
template <typename T>
class BitsetResource : public ContainerResource<std::vector<uint64_t>, BitsetResource<T>, T> {
    public:
        /// @brief Alias for the underlying word-vector container.
        using Container = std::vector<uint64_t>;
        /// @brief Logical element type (bit index type).
        using ValueType = T;
        /// @brief CRTP derived type alias.
        using Derived = BitsetResource<T>;

        /// @brief Default constructor — creates an empty bitset.
        BitsetResource() = default;

        /// @brief Constructs a BitsetResource from a set of indices.
        ///
        /// Each index in @p indices is set as a bit in the internal word vector.
        ///
        /// @param indices Set of bit indices to pre-populate.
        explicit BitsetResource(const std::set<ValueType>& indices) {
            for (auto idx : indices) {
                add(idx);
            }
        }

        /// @brief Replaces the bitset contents with the indices in @p indices.
        ///
        /// This overload is required to satisfy ResourceInitializerTypeTuple when
        /// the initializer type is `std::set<ValueType>`.
        ///
        /// @param indices New set of bit indices.
        void set_value(const std::set<ValueType>& indices) {
            this->container_.clear();
            size_ = 0;
            for (auto idx : indices) {
                add(idx);
            }
        }

        /// @brief Replaces the internal word vector and recomputes the cached size.
        ///
        /// @param container New word vector (moved in).
        void set_value(Container container) override {
            ContainerResource<std::vector<uint64_t>, BitsetResource<T>, T>::set_value(
                std::move(container));
            size_ = compute_size();
        }

        /// @brief Clears all bits and resets the element count to zero.
        void reset() {
            this->container_.clear();
            size_ = 0;
        }

        // Note: idx >> 6 is a bitwise right shift of idx by 6 bits — equivalent to integer division
        // by 64 (floor). In this bitset code it computes which 64-bit word (slot) contains bit
        // number idx. The companion idx & 63 computes idx % 64 (bit offset inside that word).
        /// @brief Sets the bit at position @p idx, growing the word vector if necessary.
        ///
        /// No-op if the bit is already set.
        ///
        /// @param idx Bit index (element) to add.
        void add(const ValueType& idx) override {
            ensure_words_size(idx + 1);
            const uint64_t bit = 1ULL << (idx & 63);    // NOLINT
            if (!(this->container_[idx >> 6] & bit)) {  // NOLINT
                this->container_[idx >> 6] |= bit;      // NOLINT
                ++size_;
            }
        }

        /// @brief ORs @p other_words into this bitset, growing if necessary.
        ///
        /// The cached element count is recomputed after the merge.
        ///
        /// @param other_words Word vector to merge in.
        void add(const Container& other_words) override {
            // OR the other words into this bitset
            const size_t other_words_count = other_words.size();
            ensure_words_size(other_words_count * 64);  // NOLINT
            for (size_t i = 0; i < other_words_count; ++i) {
                this->container_[i] |= other_words[i];
            }
            size_ = compute_size();
        }

        /// @brief Clears the bit at position @p idx if it is within bounds.
        ///
        /// @param idx Bit index to remove.
        void remove(const ValueType& idx) override {
            if (idx < 64 * this->container_.size()) {     // avoid out-of-bounds  NOLINT
                const uint64_t bit = 1ULL << (idx & 63);  // NOLINT
                if (this->container_[idx >> 6] & bit) {   // NOLINT
                    this->container_[idx >> 6] &= ~bit;   // NOLINT
                    --size_;
                }
            }
        }

        /// @brief Returns true if the bit at position @p idx is set.
        ///
        /// @param idx Bit index to test.
        /// @return True if the bit is set, false if it is clear or out of range.
        [[nodiscard]] bool contains(const ValueType& idx) const override {
            if (idx >= 64 * this->container_.size()) {  // avoid out-of-bounds  NOLINT
                return false;
            }
            return ((this->container_[idx >> 6] >> (idx & 63)) & 1ULL) != 0ULL;  // NOLINT
        }

        /// @brief Returns true if every bit set in @p other is also set in this bitset.
        ///
        /// @param other Word vector to test for subset relation.
        /// @return True if this bitset is a superset of @p other.
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

        /// @brief Returns true if this bitset and @p other share at least one set bit.
        ///
        /// @param other Word vector to test against.
        /// @return True if the bitsets are not disjoint.
        [[nodiscard]] bool intersects(const Container& other) const override {
            const size_t words = std::min(this->container_.size(), other.size());
            for (size_t i = 0; i < words; ++i) {
                if ((this->container_[i] & other[i]) != 0ULL) {
                    return true;
                }
            }
            return false;
        }

        /// @brief Returns the bitwise OR (union) of this bitset and @p other.
        ///
        /// @param other Word vector to OR with.
        /// @return New word vector with all bits set in either operand.
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

        /// @brief Returns the bitwise AND (intersection) of this bitset and @p other.
        ///
        /// The output length equals `min(this->size(), other.size())` words.
        /// Trailing zero words are kept to avoid subsequent reallocation.
        ///
        /// @param other Word vector to AND with.
        /// @return New word vector with only bits set in both operands.
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

        /// @brief Returns the bitwise AND-NOT (set difference) of this bitset minus @p other.
        ///
        /// @param other Bits to remove.
        /// @return New word vector with the bits of @p other cleared.
        [[nodiscard]] Container subtract(const Container& other) const override {
            const size_t words_this = this->container_.size();
            const size_t words_other = other.size();
            Container result(words_this, 0ULL);
            for (size_t i = 0; i < words_this; ++i) {
                const uint64_t a = this->container_[i];
                if (i >= words_other) {
                    result[i] = a;
                } else {
                    result[i] = a & ~other[i];
                }
            }
            return result;
        }

        /// @brief Returns the index of the highest set bit plus one (i.e. the minimum
        /// number of bits needed to represent the bitset), or 0 if @p bits is all-zero.
        ///
        /// @param bits Word vector to inspect.
        /// @return Number of significant bits.
        [[nodiscard]] static size_t compute_used_bits(const Container& bits) {
            for (size_t i = bits.size(); i > 0; --i) {
                const uint64_t w = bits[i - 1];
                if (w == 0ULL) {
                    continue;
                }
                const unsigned msb = 63U - static_cast<unsigned>(std::countl_zero(w));
                return (i - 1) * 64 + static_cast<size_t>(msb) + 1;  // NOLINT
            }
            return 0;
        }

        /// @brief Returns the total number of bits currently allocated (words * 64).
        ///
        /// @return Allocated bit capacity.
        [[nodiscard]] size_t compute_allocated_bits() const {
            return this->container_.size() * 64ULL;  // NOLINT
        }

        /// @brief Returns a const reference to the raw word vector.
        ///
        /// @return The underlying `std::vector<uint64_t>` word storage.
        [[nodiscard]] const Container& words() const { return this->container_; }

        /// @brief Returns the number of set bits (cached; O(1)).
        ///
        /// @return Count of elements (set bits).
        [[nodiscard]] size_t size() const override { return size_; }

        /// @brief Counts the number of set bits by scanning all words (O(words)).
        ///
        /// Used to refresh the cached size after bulk operations.
        ///
        /// @return Number of set bits in the word vector.
        [[nodiscard]] size_t compute_size() const {
            size_t size = 0;
            for (const uint64_t w : this->container_) {
                size += static_cast<size_t>(std::popcount(w));
            }
            return size;
        }

        /// @brief Returns true if no bits are set.
        ///
        /// @return True when the element count is zero.
        [[nodiscard]] bool empty() const override { return size_ == 0; }

        /// @brief Converts the bitset to an ordered `std::set<ValueType>`.
        ///
        /// @return Set containing the indices of all set bits.
        [[nodiscard]] std::set<ValueType> to_set() const {
            std::set<ValueType> result;
            for (size_t i = 0; i < this->container_.size(); ++i) {
                uint64_t w = this->container_[i];
                for (size_t bit = 0; bit < 64; ++bit) {                       // NOLINT
                    if ((w & (1ULL << bit)) != 0ULL) {                        // NOLINT
                        result.insert(static_cast<ValueType>(i * 64 + bit));  // NOLINT
                    }
                }
            }
            return result;
        }

        /// @brief Returns a human-readable string of the set bits.
        ///
        /// Delegates to ContainerResource::to_string after converting to a set.
        ///
        /// @return String of the form `{i1,i2,...}` listing all set bit indices.
        [[nodiscard]] std::string to_string() const override {
            return ContainerResource<Container, Derived, ValueType>::to_string(to_set());
        }

    private:
        // storage is inherited from ContainerResource as `container_`.
        size_t size_{0};

        void ensure_words_size(ValueType requested_nb_bits) {
            const size_t new_words = (requested_nb_bits + 63) / 64;
            if (this->container_.size() < new_words) {
                this->container_.resize(new_words, 0ULL);
            }
        }
};

}  // namespace rcspp
