// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <bit>      // NOLINT(build/include_order)
#include <cstdint>  // NOLINT(build/c++11)
#include <queue>
#include <ranges>  // NOLINT(build/include_order)
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rcspp/graph/graph.hpp"

namespace rcspp {

/**
 * @brief ConnectivityMatrix computes reachability information on a directed graph.
 *
 * This helper class encapsulates algorithms to compute the transitive closure
 * (reachability) of a directed graph as a compact bit-matrix (each row is an array of 64-bit
 * words), and
 *
 * The class stores the computed bit-matrix and associated metadata (node
 * ordering and node-id -> index map). Storing the matrix makes repeated
 * reachability queries cheap (constant-time bit test) at the expense of
 * memory proportional to O(N^2 / 64).
 *
 * Template parameter:
 *  - ResourceType: the resource type used by graph nodes (keeps API consistent
 *    with other graph utilities in the project). The class only needs the
 *    graph topology (node ids and out_arcs) so ResourceType is not inspected.
 *
 * Usage pattern:
 *  - Create an instance with a pointer to a Graph<ResourceType>.
 *  - Call compute_bitmatrix() once (or let is_connected() compute it lazily).
 *  - Use is_connected(a,b) for fast checks of reachability between nodes.
 *  - Optionally access node_ids_ and id_to_index_ for mapping node ids to
 *    matrix indices. Use bit_matrix_ for direct access to the bit-matrix if needed
 *    to obtain full matrices or source->sink maps.
 */
template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class ConnectivityMatrix {
    public:
        using GraphT = Graph<ResourceType>;

        /**
         * @brief Construct helper for a specific graph.
         * @param graph Pointer to the graph to analyze. The graph must outlive this helper.
         *
         * We store the pointer only (no ownership). All methods query topology via the
         * provided graph pointer (get_node_ids(), get_node(), out_arcs, ...).
         */
        explicit ConnectivityMatrix(const GraphT* graph) : graph_(graph) {}

        /**
         * @brief Compute and store full reachability bit-matrix for the graph.
         *
         * Implementation overview (optimized):
         * - We first obtain a stable ordering of node ids via graph->get_node_ids().
         *   This ordering defines the integer matrix indices 0..N-1 used for rows
         *   and columns throughout the function.
         * - We construct an index-based adjacency list (vector<vector<size_t>>) so
         *   the algorithm works purely on integer indices (fast and cache-friendly).
         * - We run an iterative Tarjan algorithm (no recursion) to compute strongly
         *   connected components (SCCs). Each SCC becomes a single node in the
         *   condensed graph; nodes in the same SCC have identical reachability and
         *   therefore can share a single bit-row.
         * - We build the condensed DAG over SCCs (no cycles). For each SCC we
         *   initialize a bit-row with bits set for the nodes that belong to that
         *   SCC (the "local" nodes).
         * - We compute a reverse-topological ordering of the condensed DAG and
         *   propagate reachability: processing SCCs in reverse topo order we OR the
         *   children's bit-rows into the parent's bit-row. After this step the
         *   SCC bit-row contains all nodes reachable from any node in the SCC.
         * - We store the SCC-level bit-rows in `scc_node_bits_` and the mapping
         *   node_index -> scc_id in `scc_of_node_`.
         *
         * Bit layout and mapping details:
         * - `node_ids_` is the canonical vector of node ids; its index (0..N-1)
         *   is the column/row index used inside bit-rows.
         * - Each SCC bit-row is a vector<uint64_t> of length W = ceil(N/64).
         *   Bit for column j is stored at word = j/64, bit = j%64.
         * - A set bit in scc_node_bits_[s][word] indicates that nodes of SCC s
         *   can reach the node whose index is (word*64 + bit).
         *
         * Individual nodes do not store separate rows; when answering queries we
         *   find the SCC of the node (via `scc_of_node_`) and test the bit in the
         *   SCC's row. This avoids duplicating identical rows for nodes in the
         *   same SCC and saves both memory and computation.
         *
         * Complexity and trade-offs:
         * - Tarjan (iterative): O(N + E).
         * - Building the condensed DAG: O(N + E).
         * - Propagating SCC bit-rows: O((#SCC_edges) * W) where W = ceil(N/64)
         *   is the number of 64-bit words per row. For graphs with few SCCs or
         *   where SCCs collapse many nodes this is often much cheaper than
         *   running N independent BFSs or a full Warshall-like closure.
         * - Memory: we keep one bit-row per SCC (scc_count * W words) which is
         *   typically smaller than N * W when SCCs merge nodes.
         *
         * Safety / notes:
         * - The iterative Tarjan avoids call-stack recursion and handles deep
         *   graphs safely.
         * - If the graph topology changes after computing the SCC rows you must
         *   call `compute_bitmatrix()` again to refresh `scc_node_bits_` and
         *   `scc_of_node_`.
         */
        void compute_bitmatrix() {  // NOLINT
            if (graph_ == nullptr) {
                return;
            }

            node_ids_ = graph_->get_node_ids();
            const size_t N = node_ids_.size();
            if (N == 0) {
                bit_matrix_.clear();
                id_to_index_.clear();
                scc_node_bits_.clear();
                scc_of_node_.clear();
                return;
            }

            const size_t words = (N + 63) / 64;

            id_to_index_.clear();
            id_to_index_.reserve(N * 2);
            for (size_t i = 0; i < N; ++i) {
                id_to_index_[node_ids_[i]] = i;
            }

            // Build adjacency list (indices) for the graph
            std::vector<std::vector<size_t>> adj(N);
            for (size_t i = 0; i < N; ++i) {
                const auto& node = graph_->get_node(node_ids_[i]);
                for (const auto arc_ptr : node.out_arcs) {
                    const auto it = id_to_index_.find(arc_ptr->destination->id);
                    if (it != id_to_index_.end()) {
                        adj[i].push_back(it->second);
                    }
                }
            }

            // Tarjan's algorithm (iterative) to compute strongly connected components (SCCs)
            // Iterative variant avoids recursion by simulating the call stack explicitly.
            //
            // Frame semantics and invariants:
            // - We simulate recursion with `dfs_stack` of Frame{v,next}, where `next`
            //   is the index of the next neighbor to process for node `v`.
            // - `index[v]` stores the discovery index assigned the first time `v`
            //   is visited; `low[v]` holds the smallest index reachable from `v` via
            //   DFS tree edges and back edges (Tarjan lowlink value).
            // - `stack` is the usual Tarjan stack of nodes currently in the active
            //   SCC being built; `onstack[v]` marks membership. When low[v] == index[v]
            //   we pop the stack to form an SCC.
            //
            // Why iterative: recursive Tarjan can overflow the C++ call stack on
            // very deep graphs (e.g., long chains). The explicit stack here uses
            // heap storage and is safe for larger inputs.
            std::vector index(N, -1);
            std::vector low(N, 0);
            std::vector onstack(N, 0);
            std::vector scc_id(N, -1);
            std::vector<size_t> stack;
            stack.reserve(N);  // stack of nodes currently in component
            int idx = 0;
            size_t scc_count = 0;

            // Explicit DFS stack of frames: (node, next_child_index)
            struct Frame {
                    size_t v;
                    size_t next;
            };
            std::vector<Frame> dfs_stack;
            dfs_stack.reserve(N);

            // Note: all indexing inside the function uses node *indices* (0..N-1)
            // returned by graph->get_node_ids(). The public API (is_connected,
            // compute_connectivity) accepts graph node ids; we translate to indices
            // via `id_to_index_` at query time. This distinction avoids repeated
            // map lookups during SCC computation and keeps the inner loops numeric.
            for (size_t start = 0; start < N; ++start) {
                if (index[start] != -1) {
                    continue;
                }

                // start new DFS from 'start'
                dfs_stack.push_back({start, 0});
                while (!dfs_stack.empty()) {
                    Frame& f = dfs_stack.back();
                    size_t v = f.v;

                    if (index[v] == -1) {
                        // first time we see v: assign index/low and push to SCC stack
                        index[v] = low[v] = idx++;
                        stack.push_back(v);
                        onstack[v] = 1;
                    }

                    // process next neighbor if any
                    if (f.next < adj[v].size()) {
                        size_t w = adj[v][f.next++];
                        if (index[w] == -1) {
                            // recurse to w (push new frame)
                            dfs_stack.push_back({w, 0});
                            continue;
                        }
                        if (onstack[w] > 0) {
                            // back-edge to node on stack: update lowlink
                            low[v] = std::min(low[v], index[w]);
                        }
                        // continue processing current frame (f)
                    } else {
                        // finished all children of v; pop frame
                        dfs_stack.pop_back();

                        // propagate lowlink to parent (if any)
                        if (!dfs_stack.empty()) {
                            size_t parent = dfs_stack.back().v;
                            low[parent] = std::min(low[parent], low[v]);
                        }

                        // if v is a root of SCC, pop nodes from `stack` until v
                        if (low[v] == index[v]) {
                            while (true) {
                                size_t w = stack.back();
                                stack.pop_back();
                                onstack[w] = 0;
                                scc_id[w] = scc_count;
                                if (w == v) {
                                    break;
                                }
                            }
                            ++scc_count;
                        }
                    }
                }
            }

            // Build members per SCC
            std::vector<std::vector<size_t>> scc_members(scc_count);
            for (size_t v = 0; v < N; ++v) {
                scc_members[scc_id[v]].push_back(v);
            }

            // Build condensed Directed Acyclic Graph (DAG) adjacency (SCC graph)
            std::vector<std::vector<size_t>> cond_adj(scc_count);
            std::vector<std::unordered_set<size_t>> cond_adj_set(scc_count);
            for (size_t u = 0; u < N; ++u) {
                for (size_t v : adj[u]) {
                    int su = scc_id[u];
                    int sv = scc_id[v];
                    if (su != sv && cond_adj_set[su].insert(sv).second) {
                        cond_adj[su].push_back(sv);
                    }
                }
            }

            // Prepare SCC-level bit rows: each SCC row contains bits for nodes in the SCC
            std::vector<std::vector<uint64_t>> scc_bits(scc_count,
                                                        std::vector<uint64_t>(words, 0ULL));
            for (size_t s = 0; s < scc_count; ++s) {
                for (size_t v : scc_members[s]) {
                    const size_t w = v >> 6;
                    const size_t b = v & 63;
                    scc_bits[s][w] |= (1ULL << b);
                }
            }

            // Compute reverse topological order of condensed DAG (Kahn)
            std::vector<size_t> indeg(scc_count, 0);
            for (size_t u = 0; u < scc_count; ++u) {
                for (size_t v : cond_adj[u]) {
                    ++indeg[v];
                }
            }
            std::queue<size_t> q;
            for (size_t i = 0; i < scc_count; ++i) {
                if (indeg[i] == 0) {
                    q.push(i);
                }
            }
            std::vector<size_t> topo;
            topo.reserve(scc_count);
            while (!q.empty()) {
                size_t u = q.front();
                q.pop();
                topo.push_back(u);
                for (size_t v : cond_adj[u]) {
                    if (--indeg[v] == 0) {
                        q.push(v);
                    }
                }
            }

            // Propagate reachability across the condensed DAG in reverse-topological order.
            // Each SCC's bitset becomes itself ORed with all children's bitsets.
            for (size_t u : std::ranges::reverse_view(topo)) {
                for (size_t v : cond_adj[u]) {
                    for (size_t w = 0; w < words; ++w) {
                        scc_bits[u][w] |= scc_bits[v][w];
                    }
                }
            }

            // Store SCC-level results and per-node SCC mapping to avoid per-node copies.
            scc_node_bits_.swap(scc_bits);  // move into member
            scc_of_node_.assign(N, -1);
            for (size_t v = 0; v < N; ++v) {
                scc_of_node_[v] = scc_id[v];
            }

            // Clear per-node bit_matrix_ to avoid duplication; queries use SCC-level data.
            bit_matrix_.clear();
        }

        /**
         * @brief Fast reachability query: does node with id `a` reach node `b`?
         *
         * This method is O(1) once the SCC-level bit rows have been computed
         * (it tests a single bit in the appropriate SCC row). If the matrix has
         * not been computed it will be computed lazily by calling
         * `compute_bitmatrix()`.
         *
         * Returns false on invalid graph or when either id is unknown.
         */
        [[nodiscard]] bool is_connected(size_t a, size_t b) {
            if (graph_ == nullptr) {
                return false;
            }
            if (scc_node_bits_.empty()) {
                // Lazy computation: compute on first demand
                compute_bitmatrix();
            }

            const auto ita = id_to_index_.find(a);
            const auto itb = id_to_index_.find(b);
            if (ita == id_to_index_.end() || itb == id_to_index_.end()) {
                // One of the node ids is not present in the graph ordering
                return false;
            }

            const size_t ia = ita->second;
            const size_t ib = itb->second;

            // map node indices to SCC ids
            const int scc_a = scc_of_node_.at(ia);
            // scc_node_bits_ rows are indexed by SCC id; bits correspond to node indices
            const size_t w = ib >> 6;
            const size_t bit = ib & 63;

            // Test the bit in the SCC row for a
            return ((scc_node_bits_.at(scc_a)[w] >> bit) & 1ULL) != 0ULL;
        }

        /**
         * @brief Compute reachability from source nodes to sink nodes.
         *
         * Returns a map source_node_id -> sorted vector of reachable sink node ids.
         * Uses the SCC-level bit rows if available (fast), otherwise falls back to
         * a BFS per source.
         */
        [[nodiscard]] std::unordered_map<size_t, std::vector<size_t>>
        compute_connectivity() {  // NOLINT
            if (graph_ == nullptr || !reachability_cache_.empty()) {
                return reachability_cache_;
            }

            const auto sources = graph_->get_source_node_ids();
            const auto sinks = graph_->get_sink_node_ids();

            // Ensure SCC-level bit rows are computed to enable the fast extraction
            // of sink reachability. compute_bitmatrix() is cheap for small graphs
            // and caches results for repeated queries.
            if (scc_node_bits_.empty()) {
                compute_bitmatrix();
            }
            std::unordered_set<size_t> sink_set(sinks.begin(), sinks.end());

            // Use SCC-level bit rows to extract sink reachability
            const size_t N_nodes = node_ids_.size();
            const size_t words = scc_node_bits_[0].size();

            for (size_t i = 0; i < N_nodes; ++i) {
                std::vector<size_t> reached;
                const int scc_i = scc_of_node_.at(i);
                const auto& row_bits = scc_node_bits_[scc_i];

                for (size_t w = 0; w < words; ++w) {
                    uint64_t word = row_bits[w];
                    if (word == 0ULL) {
                        continue;
                    }
                    const size_t base = w * 64;
                    while (word != 0ULL) {
                        const auto tz = static_cast<unsigned>(std::countr_zero(word));
                        const size_t j = base + tz;
                        if (j < N_nodes) {
                            const size_t node_id = node_ids_[j];
                            if (sink_set.contains(node_id)) {
                                reached.push_back(node_id);
                            }
                        }
                        word &= word - 1;  // clear lowest set bit
                    }
                }

                if (!reached.empty()) {
                    std::ranges::sort(reached);
                }

                const size_t src_id = node_ids_[i];
                if (std::ranges::find(sources, src_id) != sources.end()) {
                    reachability_cache_.emplace(src_id, std::move(reached));
                }
            }

            return reachability_cache_;
        }

    private:
        // Non-owning pointer to the graph analysed by this helper. The caller must
        // ensure the graph outlives the ConnectivityMatrix instance.
        const GraphT* graph_ = nullptr;

        // The legacy per-node compact bit-matrix (kept for compatibility). When
        // using SCC-level storage we leave this empty to avoid duplication.
        // bit_matrix_[i][w] would contain bits for columns (w*64 .. w*64+63).
        std::vector<std::vector<uint64_t>> bit_matrix_;

        // node_ids_ stores the ordering that maps matrix row/column indices to graph node ids
        std::vector<size_t> node_ids_;

        // id_to_index_ maps node id -> matrix index (0..N-1). This allows O(1)
        // translation from node id to its row/column index used in the bit-matrix.
        std::unordered_map<size_t, size_t> id_to_index_;

        // SCC-level storage: each SCC has a bit-row indicating reachable nodes.
        // - scc_node_bits_[s] is a vector<uint64_t> with W = ceil(N/64) words.
        //   Bit j (word j/64, bit j%64) == 1 means: nodes in SCC s can reach node j.
        std::vector<std::vector<uint64_t>> scc_node_bits_;

        // scc_of_node_[node_index] -> scc_id. Use this to find which SCC row
        // contains the reachability information for a given node index.
        std::vector<int> scc_of_node_;

        // Cache for connectivity map source_id -> vector<reachable_ids>
        std::unordered_map<size_t, std::vector<size_t>> reachability_cache_;
};

}  // namespace rcspp
