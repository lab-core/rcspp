// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "rcspp/graph/row.hpp"

namespace rcspp {

/// @brief FNV-1a 64-bit offset basis constant.
inline constexpr std::uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;

/// @brief FNV-1a 64-bit prime multiplier constant.
inline constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;

/// @brief Number of bytes in a 64-bit integer, used to drive the FNV-1a byte loop.
inline constexpr int FNV_NUM_BYTES_UINT64 = 8;

/// @brief Byte-extraction mask used inside the FNV-1a mixing loop.
inline constexpr std::uint64_t FNV_NUM_BITS_PER_BYTE_UINT64 = 0xFFU;

/// @brief Number of bits per byte, used to shift out processed bytes in the FNV-1a loop.
inline constexpr int FNV_NUM_BITS_PER_BYTE = 8;

/// @brief Mixes a 64-bit value @p v into an FNV-1a running hash @p h.
///
/// Processes @p v byte-by-byte using the FNV-1a algorithm: XOR each byte into
/// the running hash, then multiply by @c FNV_PRIME. This is a simple, fast,
/// non-cryptographic hash suitable for hash tables and checksums.
///
/// @param v  The 64-bit value to mix into the hash.
/// @param h  The running hash state (defaults to @c FNV_OFFSET_BASIS for a fresh hash).
/// @return   The updated hash after mixing all 8 bytes of @p v.
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

/// @brief Represents a feasible solution (path) found by the RCSPP algorithm.
///
/// A @c Solution records the total path cost, the ordered sequence of node IDs,
/// the ordered sequence of arc IDs, and an optional master-LP @c Column. It
/// maintains an FNV-1a hash of the arc-ID sequence for fast equality testing
/// inside @c std::unordered_set.
struct Solution {
        /// @brief Constructs a default, infeasible-sentinel solution.
        ///
        /// @c cost is initialised to @c +infinity and the path vectors are empty.
        Solution() noexcept { init_hash(); }

        /// @brief Constructs a fully specified solution.
        ///
        /// @param _cost          Total path cost (sum of arc costs).
        /// @param _path_node_ids Ordered sequence of node IDs along the path.
        /// @param _path_arc_ids  Ordered sequence of arc IDs along the path.
        /// @param _column        Optional master-LP column associated with this solution.
        Solution(double _cost, std::vector<size_t> _path_node_ids,
                 std::vector<size_t> _path_arc_ids, Column _column = {})
            : cost(_cost),
              path_node_ids(std::move(_path_node_ids)),
              path_arc_ids(std::move(_path_arc_ids)),
              column(std::move(_column)) {
            init_hash();
        }

        /// @brief Compares two solutions for equality.
        ///
        /// The hash serves as a fast prefilter: only when hashes match is the full
        /// arc-ID sequence compared, so genuine FNV-1a collisions do not coalesce
        /// distinct solutions in @c std::unordered_set.
        ///
        /// @param rhs The solution to compare against.
        /// @return @c true if both solutions traverse exactly the same sequence of arcs.
        bool operator==(const Solution& rhs) const noexcept {
            return hash_ == rhs.hash_ && path_arc_ids == rhs.path_arc_ids;
        }

        /// @brief Returns the FNV-1a hash of the arc-ID sequence.
        ///
        /// @return The 64-bit content hash used by the @c std::hash specialisation.
        [[nodiscard]] uint64_t get_hash() const noexcept { return hash_; }

        /// @brief Recomputes the content hash after @c path_arc_ids has been mutated.
        ///
        /// The value constructor hashes automatically. Call this method when
        /// @c path_arc_ids is modified directly (e.g., via the Python setter).
        void rehash() noexcept { init_hash(); }

        /// @brief Total cost of the path (sum of arc costs along the route).
        ///
        /// Distinct from @c column.cost, which is the master-LP reduced cost used by
        /// the pricing step. The two are normally equal but stored separately.
        /// Defaults to @c +infinity (the RCSPP infeasible/unset sentinel).
        double cost = std::numeric_limits<double>::infinity();

        /// @brief Ordered sequence of node IDs visited by the path.
        std::vector<size_t> path_node_ids;

        /// @brief Ordered sequence of arc IDs traversed by the path.
        std::vector<size_t> path_arc_ids;

        /// @brief Master-LP column associated with this solution (may be empty).
        Column column;

    private:
        std::uint64_t hash_ = 0;

        // Order-sensitive hash: different arc-id sequence -> different hash. Used as
        // the fast prefilter in operator==; the path-equality fallback there handles
        // collisions correctly, so this hash function does not need to be perfect.
        void init_hash() {
            hash_ = FNV_OFFSET_BASIS;             // initialize hash
            for (std::size_t a : path_arc_ids) {  // hash each arc id sequentially
                hash_ = fnv1a_mix_uint64(static_cast<std::uint64_t>(a), hash_);
            }
        }
};
}  // namespace rcspp

/// @brief @c std::hash specialisation for @c rcspp::Solution.
///
/// Delegates to @c Solution::get_hash() so that @c Solution objects can be stored
/// directly in @c std::unordered_set and @c std::unordered_map.
template <>
struct std::hash<rcspp::Solution> {
        /// @brief Computes the hash of a @c Solution.
        ///
        /// @param s The solution to hash.
        /// @return The FNV-1a hash of the solution's arc-ID sequence.
        size_t operator()(rcspp::Solution const& s) const noexcept { return s.get_hash(); }
};
