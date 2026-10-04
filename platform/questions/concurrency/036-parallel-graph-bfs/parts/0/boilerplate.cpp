#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

// Level-synchronous parallel BFS: process one whole frontier (all nodes at
// the current distance) in parallel, then move to the next. Returns the
// distance from `source` to every node, or -1 if unreachable.
std::vector<int> parallel_bfs(const std::vector<std::vector<int>>& adj, int source,
                               int num_threads = 8) {
    const std::size_t n = adj.size();
    std::vector<int> dist(n, -1);
    if (source < 0 || static_cast<std::size_t>(source) >= n) return dist;

    // TODO: implement
    return dist;
}
