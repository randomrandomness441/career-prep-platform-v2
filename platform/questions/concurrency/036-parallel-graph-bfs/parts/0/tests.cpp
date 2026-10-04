// Harness for "Parallel Graph BFS". Includes the candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <queue>
#include <vector>

// Known-correct sequential reference.
static std::vector<int> sequential_bfs(const std::vector<std::vector<int>>& adj, int source) {
    std::vector<int> dist(adj.size(), -1);
    if (source < 0 || static_cast<std::size_t>(source) >= adj.size()) return dist;
    dist[static_cast<std::size_t>(source)] = 0;
    std::queue<int> q;
    q.push(source);
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (int v : adj[static_cast<std::size_t>(u)]) {
            if (dist[static_cast<std::size_t>(v)] == -1) {
                dist[static_cast<std::size_t>(v)] = dist[static_cast<std::size_t>(u)] + 1;
                q.push(v);
            }
        }
    }
    return dist;
}

static bool check(const std::vector<std::vector<int>>& adj, int source, int num_threads,
                   const char* label) {
    std::vector<int> want = sequential_bfs(adj, source);
    std::vector<int> got = parallel_bfs(adj, source, num_threads);
    if (got.size() != want.size()) {
        std::printf("%s: result has %zu entries, expected %zu\n", label, got.size(), want.size());
        return false;
    }
    for (std::size_t i = 0; i < want.size(); ++i) {
        if (got[i] != want[i]) {
            std::printf("%s: dist[%zu] = %d, expected %d\n", label, i, got[i], want[i]);
            return false;
        }
    }
    return true;
}

int main() {
    // ── 1. small, deterministic sanity check ──────────────────────────────
    {
        std::vector<std::vector<int>> adj = {{1, 2}, {0, 3}, {0, 3}, {1, 2}};
        if (!check(adj, 0, 4, "small")) return 1;
    }

    // ── 2. a wide "convergence" graph: many level-1 nodes all point to the
    //      same small set of level-2 hub nodes -- this is the shape that
    //      makes many threads race to discover the SAME node at once ──────
    for (int trial = 0; trial < 6; ++trial) {
        constexpr int kSpread = 4000;   // level-1 nodes, all children of node 0
        constexpr int kHubs = 20;       // level-2 nodes, each hit by ~200 level-1 nodes
        constexpr int kLeavesPerHub = 3;

        std::vector<std::vector<int>> adj;
        adj.emplace_back();  // node 0: source
        for (int i = 0; i < kSpread; ++i) adj[0].push_back(1 + i);

        for (int i = 0; i < kSpread; ++i) {
            adj.emplace_back();
            adj[static_cast<std::size_t>(1 + i)].push_back(1 + kSpread + (i % kHubs));
        }
        int hub_base = 1 + kSpread;
        for (int h = 0; h < kHubs; ++h) adj.emplace_back();  // hub nodes, filled below
        int leaf_base = hub_base + kHubs;
        for (int h = 0; h < kHubs; ++h) {
            for (int l = 0; l < kLeavesPerHub; ++l) {
                int leaf = leaf_base + h * kLeavesPerHub + l;
                adj[static_cast<std::size_t>(hub_base + h)].push_back(leaf);
            }
        }
        for (int i = 0; i < kHubs * kLeavesPerHub; ++i) adj.emplace_back();  // leaves

        SHAKE();
        if (!check(adj, 0, 8, "convergence")) return 1;
    }

    // ── 3. disconnected components: unreachable nodes stay -1 ─────────────
    {
        std::vector<std::vector<int>> adj = {{1}, {0}, {3}, {2}};  // {0,1} and {2,3} separate
        if (!check(adj, 0, 4, "disconnected")) return 1;
    }

    std::printf("small graph, 6 trials of a 4000-node convergence graph (many threads "
                "racing to discover the same hub nodes), and disconnected components: "
                "every distance exact\n");
    return 0;
}
