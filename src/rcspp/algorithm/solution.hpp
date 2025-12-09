// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <utility>

namespace rcspp {

// 64-bit FNV-1a constants
inline constexpr std::uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
inline constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;
inline constexpr int FNV_NUM_BYTES_UINT64 = 8;
inline constexpr std::uint64_t FNV_NUM_BITS_PER_BYTE_UINT64 = 0xFFU;
inline constexpr int FNV_NUM_BITS_PER_BYTE = 8;

// Hash the raw bytes of a 64-bit integer using FNV-1a
// FNV-1a is a simple, fast, noncryptographic hash designed for hash tables and checksums.
// It processes input byte-by-byte: initialize the hash to an offset basis, for each byte XOR the
// hash with the byte, then multiply by a large prime (modulo the word size).
static std::uint64_t fnv1a_mix_uint64(std::uint64_t v, std::uint64_t h = FNV_OFFSET_BASIS) {
    for (int i = 0; i < FNV_NUM_BYTES_UINT64; ++i) {  // process 1 byte (8 bits) 8 times (64 bits)
        auto byte =
            static_cast<std::uint8_t>(v & FNV_NUM_BITS_PER_BYTE_UINT64);  // get the last byte
        v >>= FNV_NUM_BITS_PER_BYTE;                                      // remove the last byte
        h ^= byte;       // XOR the hash with the byte
        h *= FNV_PRIME;  // multiply by the FNV prime
    }
    return h;
}

struct Solution {
        Solution() = default;
        Solution(double _cost, std::list<size_t> _path_node_ids, std::list<size_t> _path_arc_ids)
            : cost(_cost),
              path_node_ids(std::move(_path_node_ids)),
              path_arc_ids(std::move(_path_arc_ids)) {
            init_hash();
        }

        bool operator==(const Solution& rhs) const noexcept { return hash_ == rhs.hash_; }

        [[nodiscard]] uint64_t get_hash() const noexcept { return hash_; }

        double cost = std::numeric_limits<double>::infinity();
        std::list<size_t> path_node_ids;
        std::list<size_t> path_arc_ids;

    private:
        std::uint64_t hash_ = 0;

        // Order-sensitive hash: different order -> different hash
        // Should not have any collisions for small sequences of arc ids
        // WARNING: Hash collisions will silently skip solutions, which can compromise correctness.
        void init_hash() {
            if (hash_ == 0) {
                hash_ = FNV_OFFSET_BASIS;             // initialize hash
                for (std::size_t a : path_arc_ids) {  // hash each arc id sequentially
                    hash_ = fnv1a_mix_uint64(static_cast<std::uint64_t>(a), hash_);
                }
            }
        }
};
}  // namespace rcspp

// hash specialization for Solution for unordered_set
template <>
struct std::hash<rcspp::Solution> {
        size_t operator()(rcspp::Solution const& s) const noexcept { return s.get_hash(); }
};
