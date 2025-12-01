// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <list>
#include <utility>

namespace rcspp {

// 64-bit FNV-1a constants
static constexpr std::uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
static constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;

// Hash the raw bytes of a 64-bit integer using FNV-1a
static std::uint64_t fnv1a_mix_uint64(std::uint64_t v, std::uint64_t h = FNV_OFFSET_BASIS) {
    for (int i = 0; i < 8; ++i) {
        auto byte = static_cast<std::uint8_t>(v & 0xFFU);
        h ^= byte;
        h *= FNV_PRIME;
        v >>= 8;
    }
    return h;
}

struct Solution {
        Solution() = default;
        Solution(double _cost, std::list<size_t> _path_node_ids, std::list<size_t> _path_arc_ids)
            : cost(_cost),
              path_node_ids(std::move(_path_node_ids)),
              path_arc_ids(std::move(_path_arc_ids)) {}

        double cost = std::numeric_limits<double>::infinity();
        std::list<size_t> path_node_ids;
        std::list<size_t> path_arc_ids;

        // Order-sensitive fingerprint: different order -> different fingerprint
        std::uint64_t get_hash() {
            if (hash_ == 0) {
                hash_ = FNV_OFFSET_BASIS;
                for (std::size_t a : path_arc_ids) {
                    hash_ = fnv1a_mix_uint64(static_cast<std::uint64_t>(a), hash_);
                }
            }

            return hash_;
        }

    private:
        std::uint64_t hash_ = 0;
};
}  // namespace rcspp
