// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Tests for SetResource, BitsetResource, ContainDominanceFunction,
// InclusionDominanceFunction, IntersectionExtensionFunction,
// UnionExtensionFunction, and SubtractExtensionFunction.

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace {

// Named constants for indices and sizes.
constexpr unsigned int kIdx0 = 0U;
constexpr unsigned int kIdx1 = 1U;
constexpr unsigned int kIdx2 = 2U;
constexpr unsigned int kIdx3 = 3U;
constexpr unsigned int kIdx5 = 5U;
constexpr unsigned int kIdx10 = 10U;
constexpr unsigned int kIdx63 = 63U;
constexpr unsigned int kIdx64 = 64U;
constexpr unsigned int kIdx65 = 65U;
constexpr unsigned int kIdx127 = 127U;
constexpr unsigned int kIdx128 = 128U;
constexpr size_t kSz2 = 2U;
constexpr size_t kSz3 = 3U;
constexpr size_t kSz4 = 4U;
// Named constants for integer values used in tests.
constexpr int kVal10 = 10;
constexpr int kVal20 = 20;
constexpr int kVal42 = 42;
// Named constants for SizeFeasibilityFunction tests.
constexpr size_t kSizeMin2 = 2U;
constexpr size_t kSizeMax5 = 5U;
constexpr size_t kSizeMin1 = 1U;
constexpr size_t kSizeMax3 = 3U;
constexpr size_t kSizeMax2 = 2U;
constexpr size_t kNodeUnknown = 99U;

}  // namespace

// ============================================================================
// SetResource<int> tests
// ============================================================================

/// @brief Default-constructed SetResource is empty.
TEST(SetResource, DefaultEmpty) {
    SetResource<int> r;
    EXPECT_TRUE(r.empty());
    EXPECT_EQ(r.size(), 0U);
}

/// @brief add(value) inserts a new element.
TEST(SetResource, AddSingleValue) {
    SetResource<int> r;
    r.add(1);
    EXPECT_TRUE(r.contains(1));
    EXPECT_FALSE(r.contains(2));
    EXPECT_EQ(r.size(), 1U);
}

/// @brief add(Container) inserts all elements from the container.
TEST(SetResource, AddContainer) {
    SetResource<int> r;
    std::set<int> vals = {1, 2, 3};
    r.add(vals);
    EXPECT_TRUE(r.contains(1));
    EXPECT_TRUE(r.contains(2));
    EXPECT_TRUE(r.contains(3));
    EXPECT_EQ(r.size(), kSz3);
}

/// @brief remove(value) erases an existing element.
TEST(SetResource, RemoveExisting) {
    SetResource<int> r;
    r.add(1);
    r.add(2);
    r.remove(1);
    EXPECT_FALSE(r.contains(1));
    EXPECT_TRUE(r.contains(2));
}

/// @brief Constructor from Container initialises with given values.
TEST(SetResource, ConstructFromContainer) {
    std::set<int> init = {1, 2, 3};
    SetResource<int> r(init);
    EXPECT_EQ(r.size(), kSz3);
    EXPECT_TRUE(r.contains(1));
}

/// @brief get_value() returns a reference to the underlying container.
TEST(SetResource, GetValue) {
    SetResource<int> r;
    r.add(kVal42);
    const auto& v = r.get_value();
    EXPECT_EQ(v.size(), 1U);
    EXPECT_TRUE(v.contains(kVal42));
}

/// @brief set_value() replaces the container.
TEST(SetResource, SetValue) {
    SetResource<int> r;
    r.add(1);
    std::set<int> new_vals = {kVal10, kVal20};
    r.set_value(new_vals);
    EXPECT_FALSE(r.contains(1));
    EXPECT_TRUE(r.contains(kVal10));
}

/// @brief reset() clears all elements.
TEST(SetResource, Reset) {
    SetResource<int> r;
    r.add(1);
    r.add(2);
    r.reset();
    EXPECT_TRUE(r.empty());
}

/// @brief includes(other) returns false when other is larger.
TEST(SetResource, IncludesFalseWhenOtherLarger) {
    SetResource<int> r;
    r.add(1);
    std::set<int> big = {1, 2};
    EXPECT_FALSE(r.includes(big));
}

/// @brief includes(other) returns true when this is a superset.
TEST(SetResource, IncludesTrueSuperset) {
    SetResource<int> r;
    r.add(1);
    r.add(2);
    r.add(3);
    std::set<int> sub = {1, 2};
    EXPECT_TRUE(r.includes(sub));
}

/// @brief includes(other) returns false when other is not a subset.
TEST(SetResource, IncludesFalseNotSubset) {
    SetResource<int> r;
    r.add(1);
    r.add(2);
    std::set<int> other = {2, 3};
    EXPECT_FALSE(r.includes(other));
}

/// @brief intersects(other) returns false when either set is empty.
TEST(SetResource, IntersectsEmptyReturnsFalse) {
    SetResource<int> r;
    r.add(1);
    std::set<int> empty;
    EXPECT_FALSE(r.intersects(empty));
    SetResource<int> r2;
    EXPECT_FALSE(r2.intersects(r.get_value()));
}

/// @brief intersects(other) returns true for overlapping sets.
TEST(SetResource, IntersectsOverlapping) {
    SetResource<int> r;
    r.add(1);
    r.add(2);
    std::set<int> other = {2, 3};
    EXPECT_TRUE(r.intersects(other));
}

/// @brief intersects(other) returns false for non-overlapping sets.
TEST(SetResource, IntersectsNonOverlapping) {
    SetResource<int> r;
    r.add(1);
    r.add(2);
    std::set<int> other = {3, 4};
    EXPECT_FALSE(r.intersects(other));
}

/// @brief intersects: exercises the *it1 < *it2 branch (advance it1).
TEST(SetResource, IntersectsAdvanceIt1) {
    SetResource<int> r;
    r.add(1);
    r.add(3);
    std::set<int> other = {2, 4};
    EXPECT_FALSE(r.intersects(other));
}

/// @brief get_union(other) returns the set union.
TEST(SetResource, GetUnion) {
    SetResource<int> r;
    r.add(1);
    r.add(2);
    std::set<int> other = {2, 3};
    auto result = r.get_union(other);
    EXPECT_EQ(result.size(), kSz3);
    EXPECT_TRUE(result.contains(1));
    EXPECT_TRUE(result.contains(2));
    EXPECT_TRUE(result.contains(3));
}

/// @brief get_intersection(other) returns the set intersection.
TEST(SetResource, GetIntersection) {
    SetResource<int> r;
    r.add(1);
    r.add(2);
    r.add(3);
    std::set<int> other = {2, 3, 4};
    auto result = r.get_intersection(other);
    EXPECT_EQ(result.size(), kSz2);
    EXPECT_TRUE(result.contains(2));
    EXPECT_TRUE(result.contains(3));
}

/// @brief subtract(other) returns the set difference.
TEST(SetResource, Subtract) {
    SetResource<int> r;
    r.add(1);
    r.add(2);
    r.add(3);
    std::set<int> other = {2};
    auto result = r.subtract(other);
    EXPECT_EQ(result.size(), kSz2);
    EXPECT_TRUE(result.contains(1));
    EXPECT_TRUE(result.contains(3));
    EXPECT_FALSE(result.contains(2));
}

/// @brief to_string() on single-element set outputs "{N}".
TEST(SetResource, ToStringSingleElement) {
    SetResource<int> r;
    r.add(1);
    EXPECT_EQ(r.to_string(), "{1}");
}

/// @brief to_string() on two-element set includes comma separator.
TEST(SetResource, ToStringMultiElement) {
    SetResource<int> r;
    r.add(1);
    r.add(2);
    const std::string s = r.to_string();
    EXPECT_NE(s.find(','), std::string::npos);
}

/// @brief to_string() on empty set outputs "{}".
TEST(SetResource, ToStringEmpty) {
    SetResource<int> r;
    EXPECT_EQ(r.to_string(), "{}");
}

// ============================================================================
// BitsetResource<unsigned int> tests
// ============================================================================

/// @brief Default-constructed BitsetResource is empty.
TEST(BitsetResource, DefaultEmpty) {
    BitsetResource<unsigned int> b;
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(b.size(), 0U);
}

/// @brief Constructor from std::set initialises bits.
TEST(BitsetResource, ConstructFromSet) {
    std::set<unsigned int> init = {kIdx1, kIdx2, kIdx3};
    BitsetResource<unsigned int> b(init);
    EXPECT_TRUE(b.contains(kIdx1));
    EXPECT_TRUE(b.contains(kIdx2));
    EXPECT_TRUE(b.contains(kIdx3));
    EXPECT_EQ(b.size(), kSz3);
}

/// @brief add(idx) sets the corresponding bit.
TEST(BitsetResource, AddSingleBit) {
    BitsetResource<unsigned int> b;
    b.add(kIdx5);
    EXPECT_TRUE(b.contains(kIdx5));
    EXPECT_FALSE(b.contains(kIdx0));
    EXPECT_EQ(b.size(), 1U);
}

/// @brief add(idx) does not double-count a bit already set.
TEST(BitsetResource, AddDuplicateBitNoDoubleCount) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx1);  // duplicate
    EXPECT_EQ(b.size(), 1U);
}

/// @brief add(idx) for idx >= 64 resizes the word vector.
TEST(BitsetResource, AddBitAbove64) {
    BitsetResource<unsigned int> b;
    b.add(kIdx65);  // second word (idx 65 → word 1, bit 1)
    EXPECT_TRUE(b.contains(kIdx65));
    EXPECT_EQ(b.size(), 1U);
}

/// @brief add(idx) for idx = 127 (last bit of second word).
TEST(BitsetResource, AddBitAt127) {
    BitsetResource<unsigned int> b;
    b.add(kIdx127);
    EXPECT_TRUE(b.contains(kIdx127));
}

/// @brief add(Container& words) ORs other words into this bitset.
TEST(BitsetResource, AddWords) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    // Add a word vector with bit 2 set
    std::vector<uint64_t> other_words = {4ULL};  // 4 = bit 2
    b.add(other_words);
    EXPECT_TRUE(b.contains(kIdx1));
    EXPECT_TRUE(b.contains(kIdx2));
}

/// @brief remove(idx) clears the corresponding bit.
TEST(BitsetResource, RemoveExistingBit) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx2);
    b.remove(kIdx1);
    EXPECT_FALSE(b.contains(kIdx1));
    EXPECT_TRUE(b.contains(kIdx2));
    EXPECT_EQ(b.size(), 1U);
}

/// @brief remove(idx) is a no-op for out-of-bounds idx.
TEST(BitsetResource, RemoveOutOfBoundsNoOp) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.remove(kIdx128);  // out of bounds - no-op
    EXPECT_TRUE(b.contains(kIdx1));
    EXPECT_EQ(b.size(), 1U);
}

/// @brief remove(idx) is a no-op for a bit that is not set.
TEST(BitsetResource, RemoveUnsetBitNoOp) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.remove(kIdx2);  // kIdx2 not in b
    EXPECT_EQ(b.size(), 1U);
}

/// @brief contains(idx) returns false for out-of-bounds idx.
TEST(BitsetResource, ContainsOutOfBoundsReturnsFalse) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    EXPECT_FALSE(b.contains(kIdx128));  // out of bounds
}

/// @brief set_value(const std::set<T>&) replaces the bitset contents.
TEST(BitsetResource, SetValueFromSet) {
    BitsetResource<unsigned int> b;
    b.add(kIdx0);
    std::set<unsigned int> new_vals = {kIdx2, kIdx3};
    b.set_value(new_vals);
    EXPECT_FALSE(b.contains(kIdx0));
    EXPECT_TRUE(b.contains(kIdx2));
    EXPECT_TRUE(b.contains(kIdx3));
    EXPECT_EQ(b.size(), kSz2);
}

/// @brief set_value(Container) replaces the words vector and recounts size.
TEST(BitsetResource, SetValueFromWords) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    // 3 = bits 0 and 1 set
    std::vector<uint64_t> words = {3ULL};
    b.set_value(words);
    EXPECT_TRUE(b.contains(kIdx0));
    EXPECT_TRUE(b.contains(kIdx1));
    EXPECT_EQ(b.size(), kSz2);
}

/// @brief reset() clears all bits and resets size to 0.
TEST(BitsetResource, Reset) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx2);
    b.reset();
    EXPECT_TRUE(b.empty());
    EXPECT_EQ(b.size(), 0U);
}

/// @brief includes(other_words) returns true when this is a superset.
TEST(BitsetResource, IncludesSuperset) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx2);
    b.add(kIdx3);
    BitsetResource<unsigned int> sub;
    sub.add(kIdx1);
    EXPECT_TRUE(b.includes(sub.get_value()));
}

/// @brief includes(other_words) returns false when this is missing a bit.
TEST(BitsetResource, IncludesMissingBit) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    BitsetResource<unsigned int> other;
    other.add(kIdx1);
    other.add(kIdx2);
    EXPECT_FALSE(b.includes(other.get_value()));
}

/// @brief includes(other_words) handles other having more words than this.
TEST(BitsetResource, IncludesOtherLarger) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    BitsetResource<unsigned int> other;
    other.add(kIdx65);  // word 1 — b has only word 0
    EXPECT_FALSE(b.includes(other.get_value()));
}

/// @brief intersects(other_words) returns true for overlapping bitsets.
TEST(BitsetResource, IntersectsOverlapping) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx2);
    BitsetResource<unsigned int> other;
    other.add(kIdx2);
    other.add(kIdx3);
    EXPECT_TRUE(b.intersects(other.get_value()));
}

/// @brief intersects(other_words) returns false for non-overlapping bitsets.
TEST(BitsetResource, IntersectsNonOverlapping) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    BitsetResource<unsigned int> other;
    other.add(kIdx2);
    EXPECT_FALSE(b.intersects(other.get_value()));
}

/// @brief get_union(other_words) returns the bitwise OR.
TEST(BitsetResource, GetUnion) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    BitsetResource<unsigned int> other;
    other.add(kIdx2);
    auto result_words = b.get_union(other.get_value());
    BitsetResource<unsigned int> result;
    result.set_value(result_words);
    EXPECT_TRUE(result.contains(kIdx1));
    EXPECT_TRUE(result.contains(kIdx2));
}

/// @brief get_union with different-length word vectors covers padding branch.
TEST(BitsetResource, GetUnionDifferentWordCounts) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    BitsetResource<unsigned int> other;
    other.add(kIdx65);  // word 1
    auto result_words = b.get_union(other.get_value());
    BitsetResource<unsigned int> result;
    result.set_value(result_words);
    EXPECT_TRUE(result.contains(kIdx1));
    EXPECT_TRUE(result.contains(kIdx65));
}

/// @brief get_intersection(other_words) returns the bitwise AND.
TEST(BitsetResource, GetIntersection) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx2);
    BitsetResource<unsigned int> other;
    other.add(kIdx2);
    other.add(kIdx3);
    auto result_words = b.get_intersection(other.get_value());
    BitsetResource<unsigned int> result;
    result.set_value(result_words);
    EXPECT_TRUE(result.contains(kIdx2));
    EXPECT_FALSE(result.contains(kIdx1));
}

/// @brief subtract(other_words) clears bits present in other.
TEST(BitsetResource, Subtract) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx2);
    BitsetResource<unsigned int> other;
    other.add(kIdx2);
    auto result_words = b.subtract(other.get_value());
    BitsetResource<unsigned int> result;
    result.set_value(result_words);
    EXPECT_TRUE(result.contains(kIdx1));
    EXPECT_FALSE(result.contains(kIdx2));
}

/// @brief subtract with other having fewer words than this (covers i >= words_other branch).
TEST(BitsetResource, SubtractOtherShorter) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx65);  // word 1
    // other has only word 0
    BitsetResource<unsigned int> other;
    other.add(kIdx1);
    auto result_words = b.subtract(other.get_value());
    BitsetResource<unsigned int> result;
    result.set_value(result_words);
    EXPECT_FALSE(result.contains(kIdx1));
    EXPECT_TRUE(result.contains(kIdx65));
}

/// @brief compute_used_bits returns 0 for an empty word vector.
TEST(BitsetResource, ComputeUsedBitsEmpty) {
    std::vector<uint64_t> empty;
    EXPECT_EQ(BitsetResource<unsigned int>::compute_used_bits(empty), 0U);
}

/// @brief compute_used_bits returns correct value for a non-empty bitset.
TEST(BitsetResource, ComputeUsedBitsNonEmpty) {
    BitsetResource<unsigned int> b;
    b.add(kIdx5);
    // Highest set bit is 5, so compute_used_bits should return 6.
    const size_t used = BitsetResource<unsigned int>::compute_used_bits(b.get_value());
    EXPECT_EQ(used, static_cast<size_t>(kIdx5 + kIdx1));
}

/// @brief compute_used_bits with bit in second word.
TEST(BitsetResource, ComputeUsedBitsSecondWord) {
    BitsetResource<unsigned int> b;
    b.add(kIdx65);
    // Highest set bit is 65, so compute_used_bits returns 66.
    const size_t used = BitsetResource<unsigned int>::compute_used_bits(b.get_value());
    EXPECT_EQ(used, static_cast<size_t>(kIdx65 + kIdx1));
}

/// @brief compute_used_bits skips trailing zero words.
TEST(BitsetResource, ComputeUsedBitsSkipsTrailingZeros) {
    // Construct a word vector with bit 1 set in word 0 and word 1 = 0.
    std::vector<uint64_t> words = {2ULL, 0ULL};  // bit 1 set, trailing zero word
    const size_t used = BitsetResource<unsigned int>::compute_used_bits(words);
    EXPECT_EQ(used, kSz2);  // bit 1 → position 2
}

/// @brief compute_allocated_bits returns words * 64.
TEST(BitsetResource, ComputeAllocatedBits) {
    BitsetResource<unsigned int> b;
    b.add(kIdx65);
    EXPECT_EQ(b.compute_allocated_bits(), kSz2 * kIdx64);
}

/// @brief words() returns a reference to the internal word vector.
TEST(BitsetResource, WordsRef) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    const auto& w = b.words();
    EXPECT_FALSE(w.empty());
}

/// @brief compute_size() matches size() after construction.
TEST(BitsetResource, ComputeSizeMatchesSize) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx2);
    EXPECT_EQ(b.compute_size(), b.size());
}

/// @brief to_set() returns the set of set-bit indices.
TEST(BitsetResource, ToSet) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx2);
    const auto s = b.to_set();
    EXPECT_TRUE(s.contains(kIdx1));
    EXPECT_TRUE(s.contains(kIdx2));
    EXPECT_EQ(s.size(), kSz2);
}

/// @brief to_string() on non-empty BitsetResource is non-empty.
TEST(BitsetResource, ToStringNonEmpty) {
    BitsetResource<unsigned int> b;
    b.add(kIdx1);
    b.add(kIdx2);
    const std::string s = b.to_string();
    EXPECT_FALSE(s.empty());
    EXPECT_NE(s, "{}");
}

// ============================================================================
// ContainDominanceFunction<SetResource<int>> tests
// ============================================================================

/// @brief check_dominance: lhs dominates rhs when lhs includes rhs's container.
TEST(ContainDominanceFunction, DominatesWhenIncludes) {
    ContainDominanceFunction<SetResource<int>> fn;
    SetResource<int> lhs;
    lhs.add(1);
    lhs.add(2);
    lhs.add(3);
    SetResource<int> rhs;
    rhs.add(1);
    rhs.add(2);
    EXPECT_TRUE(fn.check_dominance(lhs, rhs));
}

/// @brief check_dominance: lhs does not dominate when it doesn't include rhs.
TEST(ContainDominanceFunction, DoesNotDominateWhenMissingElement) {
    ContainDominanceFunction<SetResource<int>> fn;
    SetResource<int> lhs;
    lhs.add(1);
    SetResource<int> rhs;
    rhs.add(1);
    rhs.add(2);
    EXPECT_FALSE(fn.check_dominance(lhs, rhs));
}

/// @brief fast_check_dominance: rhs.size() <= lhs.size() + delta.
TEST(ContainDominanceFunction, FastCheckDominance) {
    ContainDominanceFunction<SetResource<int>> fn;
    SetResource<int> lhs;
    lhs.add(1);
    lhs.add(2);
    SetResource<int> rhs;
    rhs.add(1);
    EXPECT_TRUE(fn.fast_check_dominance(lhs, rhs, 0.0));
    EXPECT_FALSE(fn.fast_check_dominance(rhs, lhs, 0.0));
}

// ============================================================================
// InclusionDominanceFunction<SetResource<int>> tests
// ============================================================================

/// @brief check_dominance: lhs dominates rhs when rhs includes lhs's container.
TEST(InclusionDominanceFunction, DominatesWhenRhsIncludesLhs) {
    InclusionDominanceFunction<SetResource<int>> fn;
    SetResource<int> lhs;
    lhs.add(1);
    SetResource<int> rhs;
    rhs.add(1);
    rhs.add(2);
    EXPECT_TRUE(fn.check_dominance(lhs, rhs));
}

/// @brief check_dominance: lhs does not dominate when rhs does not include lhs.
TEST(InclusionDominanceFunction, DoesNotDominate) {
    InclusionDominanceFunction<SetResource<int>> fn;
    SetResource<int> lhs;
    lhs.add(1);
    lhs.add(2);
    SetResource<int> rhs;
    rhs.add(1);
    EXPECT_FALSE(fn.check_dominance(lhs, rhs));
}

/// @brief fast_check_dominance: lhs.size() <= rhs.size() + delta.
TEST(InclusionDominanceFunction, FastCheckDominance) {
    InclusionDominanceFunction<SetResource<int>> fn;
    SetResource<int> lhs;
    lhs.add(1);
    SetResource<int> rhs;
    rhs.add(1);
    rhs.add(2);
    EXPECT_TRUE(fn.fast_check_dominance(lhs, rhs, 0.0));
    EXPECT_FALSE(fn.fast_check_dominance(rhs, lhs, 0.0));
}

// ============================================================================
// IntersectionExtensionFunction<SetResource<int>> tests
// ============================================================================

/// @brief extend() sets the extended resource to the intersection of two sets.
TEST(IntersectionExtensionFunction, ExtendSetsIntersection) {
    IntersectionExtensionFunction<SetResource<int>> fn;
    SetResource<int> resource;
    resource.add(1);
    resource.add(2);
    resource.add(3);
    SetResource<int> extender;
    extender.add(2);
    extender.add(3);
    extender.add(4);
    SetResource<int> result;
    fn.extend(resource, extender, &result);
    EXPECT_TRUE(result.contains(2));
    EXPECT_TRUE(result.contains(3));
    EXPECT_FALSE(result.contains(1));
    EXPECT_FALSE(result.contains(4));
}

// ============================================================================
// UnionExtensionFunction<SetResource<int>> tests
// ============================================================================

/// @brief extend() sets the extended resource to the union of two sets.
TEST(UnionExtensionFunction, ExtendSetsUnion) {
    UnionExtensionFunction<SetResource<int>> fn;
    SetResource<int> resource;
    resource.add(1);
    resource.add(2);
    SetResource<int> extender;
    extender.add(2);
    extender.add(3);
    SetResource<int> result;
    fn.extend(resource, extender, &result);
    EXPECT_TRUE(result.contains(1));
    EXPECT_TRUE(result.contains(2));
    EXPECT_TRUE(result.contains(3));
    EXPECT_EQ(result.size(), kSz3);
}

// ============================================================================
// SubtractExtensionFunction<SetResource<int>> tests
// ============================================================================

/// @brief extend() sets the extended resource to the set difference.
TEST(SubtractExtensionFunction, ExtendSetsDifference) {
    SubtractExtensionFunction<SetResource<int>> fn;
    SetResource<int> resource;
    resource.add(1);
    resource.add(2);
    resource.add(3);
    SetResource<int> extender;
    extender.add(2);
    SetResource<int> result;
    fn.extend(resource, extender, &result);
    EXPECT_TRUE(result.contains(1));
    EXPECT_FALSE(result.contains(2));
    EXPECT_TRUE(result.contains(3));
    EXPECT_EQ(result.size(), kSz2);
}

// ============================================================================
// BitsetResource with dominance/extension functions
// ============================================================================

/// @brief ContainDominanceFunction on BitsetResource.
TEST(ContainDominanceFunctionBitset, DominatesSuperset) {
    ContainDominanceFunction<BitsetResource<unsigned int>> fn;
    BitsetResource<unsigned int> lhs;
    lhs.add(kIdx1);
    lhs.add(kIdx2);
    lhs.add(kIdx3);
    BitsetResource<unsigned int> rhs;
    rhs.add(kIdx1);
    rhs.add(kIdx2);
    EXPECT_TRUE(fn.check_dominance(lhs, rhs));
    EXPECT_FALSE(fn.check_dominance(rhs, lhs));
}

/// @brief IntersectionExtensionFunction on BitsetResource.
TEST(IntersectionExtensionFunctionBitset, ExtendSetsIntersection) {
    IntersectionExtensionFunction<BitsetResource<unsigned int>> fn;
    BitsetResource<unsigned int> resource;
    resource.add(kIdx1);
    resource.add(kIdx2);
    BitsetResource<unsigned int> extender;
    extender.add(kIdx2);
    extender.add(kIdx3);
    BitsetResource<unsigned int> result;
    fn.extend(resource, extender, &result);
    EXPECT_TRUE(result.contains(kIdx2));
    EXPECT_FALSE(result.contains(kIdx1));
    EXPECT_FALSE(result.contains(kIdx3));
}

/// @brief UnionExtensionFunction on BitsetResource.
TEST(UnionExtensionFunctionBitset, ExtendSetsUnion) {
    UnionExtensionFunction<BitsetResource<unsigned int>> fn;
    BitsetResource<unsigned int> resource;
    resource.add(kIdx1);
    BitsetResource<unsigned int> extender;
    extender.add(kIdx2);
    BitsetResource<unsigned int> result;
    fn.extend(resource, extender, &result);
    EXPECT_TRUE(result.contains(kIdx1));
    EXPECT_TRUE(result.contains(kIdx2));
}

/// @brief SubtractExtensionFunction on BitsetResource.
TEST(SubtractExtensionFunctionBitset, ExtendSetsDifference) {
    SubtractExtensionFunction<BitsetResource<unsigned int>> fn;
    BitsetResource<unsigned int> resource;
    resource.add(kIdx1);
    resource.add(kIdx2);
    BitsetResource<unsigned int> extender;
    extender.add(kIdx1);
    BitsetResource<unsigned int> result;
    fn.extend(resource, extender, &result);
    EXPECT_FALSE(result.contains(kIdx1));
    EXPECT_TRUE(result.contains(kIdx2));
}

/// @brief InclusionDominanceFunction on BitsetResource.
TEST(InclusionDominanceFunctionBitset, DominatesSubset) {
    InclusionDominanceFunction<BitsetResource<unsigned int>> fn;
    BitsetResource<unsigned int> lhs;
    lhs.add(kIdx1);
    BitsetResource<unsigned int> rhs;
    rhs.add(kIdx1);
    rhs.add(kIdx2);
    EXPECT_TRUE(fn.check_dominance(lhs, rhs));
    EXPECT_FALSE(fn.check_dominance(rhs, lhs));
}

// ============================================================================
// TrivialCostFunction tests
// ============================================================================

/// @brief TrivialCostFunction always returns 0 regardless of resource content.
TEST(TrivialCostFunction, GetCostAlwaysReturnsZero) {
    TrivialCostFunction<SetResource<int>> fn;
    SetResource<int> r;
    r.add(1);
    r.add(2);
    EXPECT_DOUBLE_EQ(fn.get_cost(r), 0.0);
}

/// @brief TrivialCostFunction returns 0 on an empty resource too.
TEST(TrivialCostFunction, GetCostEmptyResourceReturnsZero) {
    TrivialCostFunction<SetResource<int>> fn;
    SetResource<int> r;
    EXPECT_DOUBLE_EQ(fn.get_cost(r), 0.0);
}

// ============================================================================
// SizeFeasibilityFunction tests
// ============================================================================

/// @brief is_feasible returns false when size is below min.
TEST(SizeFeasibilityFunction, IsFeasibleBelowMin) {
    SizeFeasibilityFunction<SetResource<int>> fn(kSizeMin2, kSizeMax5);
    SetResource<int> r;
    r.add(1);
    EXPECT_FALSE(fn.is_feasible(r));  // size 1 < min 2
}

/// @brief is_feasible returns true when size is within [min, max].
TEST(SizeFeasibilityFunction, IsFeasibleWithinRange) {
    SizeFeasibilityFunction<SetResource<int>> fn(kSizeMin1, kSizeMax3);
    SetResource<int> r;
    r.add(1);
    r.add(2);
    EXPECT_TRUE(fn.is_feasible(r));  // size 2 in [1,3]
}

/// @brief is_feasible returns false when size exceeds max.
TEST(SizeFeasibilityFunction, IsFeasibleAboveMax) {
    SizeFeasibilityFunction<SetResource<int>> fn(0, kSizeMax2);
    SetResource<int> r;
    r.add(1);
    r.add(2);
    r.add(kVal42);
    EXPECT_FALSE(fn.is_feasible(r));  // size 3 > max 2
}

/// @brief Constructor with non-empty map initialises correctly.
///        reset(node_id) with a known node applies per-node bounds.
TEST(SizeFeasibilityFunction, PreprocessAppliesNodeBounds) {
    std::map<size_t, std::pair<size_t, size_t>> bounds = {{1U, {kSizeMax2, kSizeMax2}}};
    SizeFeasibilityFunction<SetResource<int>> fn(0, kSizeMax5, bounds);
    SetResource<int> r;
    r.add(1);
    r.add(2);
    // Default [0,5]: size 2 is feasible.
    EXPECT_TRUE(fn.is_feasible(r));
    // After reset to node 1 bounds [2,2]: size 2 is exactly at limit.
    fn.reset(1);
    EXPECT_TRUE(fn.is_feasible(r));
    r.add(kVal42);
    EXPECT_FALSE(fn.is_feasible(r));  // size 3 > max 2
    // After reset to unknown node → reverts to defaults [0,5].
    fn.reset(kNodeUnknown);
    EXPECT_TRUE(fn.is_feasible(r));  // size 3 in [0,5]
}

/// @brief reset on a function with null map (empty default map) returns early.
TEST(SizeFeasibilityFunction, PreprocessNullMapReturnsEarly) {
    // Constructor 1 with empty map → min_max_size_by_node_id_ == nullptr.
    SizeFeasibilityFunction<SetResource<int>> fn(kSizeMin1, kSizeMax3);
    SetResource<int> r;
    r.add(1);
    fn.reset(0);                     // Should return early (null map branch).
    EXPECT_TRUE(fn.is_feasible(r));  // Bounds unchanged: size 1 in [1,3].
}

// ============================================================================
// assign_union / intersect_with -- the in-place twins
// ============================================================================

// They exist only to avoid two allocations per ng label extension, so the property that matters is
// that they are INDISTINGUISHABLE from the allocating pair they replace. Not "equivalent as sets":
// identical, down to the word count a bitset keeps, because `NgPathExtensionFunction` produces
// values that `DisjointMergeForm`, `InclusionDominanceFunction` and `SizeFeasibilityFunction` all
// read afterwards.
//
// Swept over a grid rather than spot-checked: the interesting cases are the ragged ones, where the
// two operands have different word counts and the shorter one decides the result's length.

namespace container_inplace_test {

/// @brief The operand sets the sweep pairs up, chosen to straddle several word boundaries.
inline const std::vector<std::set<size_t>>& samples() {
    static const std::vector<std::set<size_t>> values{
        {},
        {kIdx0},
        {kIdx1, kIdx2},
        {kIdx63},
        {kIdx64},
        {kIdx0, kIdx63, kIdx64, kIdx65},
        {kIdx127},
        {kIdx128},
        {kIdx0, kIdx1, kIdx2, kIdx3, kIdx5, kIdx10},
        {kIdx65, kIdx127, kIdx128},
    };
    return values;
}

}  // namespace container_inplace_test

/// @brief BitsetResource: assign_union matches get_union, word for word, and keeps size() right.
TEST(BitsetResourceInPlace, AssignUnionMatchesGetUnion) {
    for (const auto& lhs_values : container_inplace_test::samples()) {
        for (const auto& rhs_values : container_inplace_test::samples()) {
            SizeTBitsetResource lhs;
            lhs.set_value(lhs_values);
            SizeTBitsetResource rhs;
            rhs.set_value(rhs_values);

            // The allocating pair, exactly as NgPathExtensionFunction::apply used to spell it.
            SizeTBitsetResource expected;
            expected.set_value(rhs.get_union(lhs.get_value()));

            // A destination with unrelated contents, so a stale word would show.
            SizeTBitsetResource actual;
            actual.set_value(std::set<size_t>{kIdx0, kIdx10, kIdx127});
            actual.assign_union(lhs.get_value(), rhs.get_value());

            EXPECT_EQ(actual.get_value(), expected.get_value());
            EXPECT_EQ(actual.size(), expected.size()) << "the cached element count drifted";
        }
    }
}

/// @brief BitsetResource: intersect_with matches get_intersection, including the word count.
TEST(BitsetResourceInPlace, IntersectWithMatchesGetIntersection) {
    for (const auto& lhs_values : container_inplace_test::samples()) {
        for (const auto& mask_values : container_inplace_test::samples()) {
            SizeTBitsetResource mask;
            mask.set_value(mask_values);

            SizeTBitsetResource expected;
            expected.set_value(lhs_values);
            expected.set_value(expected.get_intersection(mask.get_value()));

            SizeTBitsetResource actual;
            actual.set_value(lhs_values);
            actual.intersect_with(mask.get_value());

            EXPECT_EQ(actual.get_value(), expected.get_value());
            EXPECT_EQ(actual.size(), expected.size()) << "the cached element count drifted";
        }
    }
}

/// @brief The two composed are the ng formula, and must equal the pair they replaced.
TEST(BitsetResourceInPlace, TheNgFormulaIsUnchanged) {
    for (const auto& memory_values : container_inplace_test::samples()) {
        for (const auto& mask_values : container_inplace_test::samples()) {
            SizeTBitsetResource memory;
            memory.set_value(memory_values);
            SizeTBitsetResource node_left;
            node_left.set_value(std::set<size_t>{kIdx2});
            SizeTBitsetResource arrival;
            arrival.set_value(mask_values);

            SizeTBitsetResource expected;
            expected.set_value(node_left.get_union(memory.get_value()));
            expected.set_value(expected.get_intersection(arrival.get_value()));

            SizeTBitsetResource actual;
            actual.assign_union(memory.get_value(), node_left.get_value());
            actual.intersect_with(arrival.get_value());

            EXPECT_EQ(actual.get_value(), expected.get_value());
            EXPECT_EQ(actual.size(), expected.size());
        }
    }
}

/// @brief SetResource: the same two properties, on the node-based container.
TEST(SetResourceInPlace, AssignUnionAndIntersectWithMatchTheAllocatingPair) {
    const std::vector<std::set<int>> samples{
        {},
        {1},
        {1, 2},
        {2, 3},
        {5},
        {1, 3, 5, 7},
        {2, 4, 6},
        {7, 8, 9},
    };

    for (const auto& lhs_values : samples) {
        for (const auto& rhs_values : samples) {
            SetResource<int> lhs(lhs_values);
            SetResource<int> rhs(rhs_values);

            SetResource<int> expected;
            expected.set_value(rhs.get_union(lhs.get_value()));

            SetResource<int> actual({42, 43});  // unrelated, so leftovers would show
            actual.assign_union(lhs.get_value(), rhs.get_value());
            EXPECT_EQ(actual.get_value(), expected.get_value());

            SetResource<int> masked_expected(lhs_values);
            masked_expected.set_value(masked_expected.get_intersection(rhs.get_value()));

            SetResource<int> masked_actual(lhs_values);
            masked_actual.intersect_with(rhs.get_value());
            EXPECT_EQ(masked_actual.get_value(), masked_expected.get_value());
        }
    }
}

/// @brief intersect_with tolerates a mask that aliases the resource's own value.
///
/// Asserted because the doc promises it -- an intersection only shrinks, so it cannot reallocate
/// and invalidate the reference. `assign_union` makes no such promise and is guarded by an assert
/// instead, which is why there is no twin for it here.
TEST(BitsetResourceInPlace, IntersectWithAcceptsAnAliasedMask) {
    SizeTBitsetResource resource;
    resource.set_value(std::set<size_t>{kIdx1, kIdx64, kIdx127});
    const std::set<size_t> before{kIdx1, kIdx64, kIdx127};

    resource.intersect_with(resource.get_value());  // x & x == x

    SizeTBitsetResource expected;
    expected.set_value(before);
    EXPECT_EQ(resource.get_value(), expected.get_value());
    EXPECT_EQ(resource.size(), kSz3);
}

/// @brief The base-class fallbacks, exercised through a container that does not override them.
///
/// `SetResource` and `BitsetResource` both override `assign_union` / `intersect_with`, so nothing
/// else reaches `ContainerResource`'s defaults -- the coverage report is what pointed this out.
/// They are the path a *third-party* container takes, and the contract they have to meet is the
/// same one the overrides do: produce what `get_union` / `get_intersection` would have produced.
/// A default that drifted from that would be discovered by whoever wrote the container, which is
/// the worst place to discover it.
TEST(ContainerResourceDefaults, FallbacksMatchTheAllocatingPair) {
    // The minimum a container has to implement. Deliberately does NOT override assign_union or
    // intersect_with, so the base versions run.
    class PlainSet : public ContainerResource<std::set<int>, PlainSet, int> {
        public:
            using Container = std::set<int>;

            void add(const int& value) override { container_.insert(value); }
            void add(const Container& other) override {
                container_.insert(other.begin(), other.end());
            }
            void remove(const int& value) override { container_.erase(value); }

            [[nodiscard]] bool contains(const int& value) const override {
                return container_.contains(value);
            }
            [[nodiscard]] bool includes(const Container& other) const override {
                return std::includes(container_.begin(),
                                     container_.end(),
                                     other.begin(),
                                     other.end());
            }
            [[nodiscard]] bool intersects(const Container& other) const override {
                Container shared;
                std::set_intersection(container_.begin(),
                                      container_.end(),
                                      other.begin(),
                                      other.end(),
                                      std::inserter(shared, shared.begin()));
                return !shared.empty();
            }
            [[nodiscard]] Container get_union(const Container& other) const override {
                Container result;
                std::set_union(container_.begin(),
                               container_.end(),
                               other.begin(),
                               other.end(),
                               std::inserter(result, result.begin()));
                return result;
            }
            [[nodiscard]] Container get_intersection(const Container& other) const override {
                Container result;
                std::set_intersection(container_.begin(),
                                      container_.end(),
                                      other.begin(),
                                      other.end(),
                                      std::inserter(result, result.begin()));
                return result;
            }
            [[nodiscard]] Container subtract(const Container& other) const override {
                Container result;
                std::set_difference(container_.begin(),
                                    container_.end(),
                                    other.begin(),
                                    other.end(),
                                    std::inserter(result, result.begin()));
                return result;
            }
    };

    const std::vector<std::set<int>> samples{{}, {1}, {1, 2}, {2, 3}, {1, 3, 5}, {4, 5, 6}};

    for (const auto& lhs : samples) {
        for (const auto& rhs : samples) {
            PlainSet expected_union;
            expected_union.set_value(lhs);
            PlainSet actual_union;
            actual_union.set_value({99});  // unrelated, so a leftover would show
            actual_union.assign_union(lhs, rhs);
            EXPECT_EQ(actual_union.get_value(), expected_union.get_union(rhs));

            PlainSet expected_masked;
            expected_masked.set_value(lhs);
            PlainSet actual_masked;
            actual_masked.set_value(lhs);
            actual_masked.intersect_with(rhs);
            EXPECT_EQ(actual_masked.get_value(), expected_masked.get_intersection(rhs));
        }
    }
}
