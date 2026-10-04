#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

// Level-synchronous parallel BFS: process one whole frontier (all nodes at
// the current distance) in parallel, build the next frontier, synchronize,
// repeat. Two things need care that a sequential BFS never has to think
// about: two threads can discover the SAME unvisited neighbour from two
// different frontier nodes at once, and many threads building "the next
// frontier" can't all be appending to one shared vector at the same time.
std::vector<int> parallel_bfs(const std::vector<std::vector<int>>& adj, int source,
                               int num_threads = 8) {
    const std::size_t n = adj.size();
    std::vector<int> dist(n, -1);
    if (source < 0 || static_cast<std::size_t>(source) >= n) return dist;

    // Each node is claimed exactly once via one atomic exchange -- whichever
    // thread flips it from false to true is the only one that ever adds
    // this node to a frontier, no matter how many threads reach it at once.
    std::vector<std::atomic<bool>> claimed(n);
    for (auto& c : claimed) c.store(false, std::memory_order_relaxed);
    claimed[static_cast<std::size_t>(source)].store(true, std::memory_order_relaxed);
    dist[static_cast<std::size_t>(source)] = 0;

    std::vector<int> frontier = {source};
    int level = 0;

    while (!frontier.empty()) {
        int nt = std::max(1, std::min<int>(num_threads, static_cast<int>(frontier.size())));
        std::size_t chunk = (frontier.size() + nt - 1) / nt;

        // Each thread discovers into its OWN local vector -- nothing shared
        // is written to during this parallel pass except `claimed` (via
        // atomic exchange) and `dist` (each index written by at most one
        // thread, since `claimed` already made that guarantee).
        std::vector<std::vector<int>> local_next(nt);
        {
            std::vector<std::thread> threads;
            for (int t = 0; t < nt; ++t) {
                std::size_t lo = static_cast<std::size_t>(t) * chunk;
                std::size_t hi = std::min(lo + chunk, frontier.size());
                threads.emplace_back([&, t, lo, hi] {
                    for (std::size_t i = lo; i < hi; ++i) {
                        int u = frontier[i];
                        for (int v : adj[static_cast<std::size_t>(u)]) {
                            bool expected = false;
                            if (claimed[static_cast<std::size_t>(v)]
                                    .compare_exchange_strong(expected, true,
                                                              std::memory_order_relaxed)) {
                                dist[static_cast<std::size_t>(v)] = level + 1;
                                local_next[t].push_back(v);
                            }
                        }
                    }
                });
            }
            for (auto& th : threads) th.join();
        }

        // Merge: cheap, sequential, and the only place the next frontier's
        // single shared vector is ever written to.
        std::vector<int> next_frontier;
        for (auto& v : local_next)
            next_frontier.insert(next_frontier.end(), v.begin(), v.end());

        frontier = std::move(next_frontier);
        ++level;
    }

    return dist;
}
