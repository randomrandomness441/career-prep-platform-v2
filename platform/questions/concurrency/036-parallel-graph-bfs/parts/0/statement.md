# Parallel Graph BFS

## ELI5: a rumor spreading through a crowd, ring by ring

Someone starts a rumor. First, they tell everyone standing right next to them, that's
"distance 1." Once *everyone* at distance 1 has heard it, each of them tells everyone
*they* know who hasn't heard it yet, that's distance 2. And so on, ring by ring,
outward. The rule that makes this level-synchronous BFS instead of chaos: nobody starts
telling the next ring until the *entire current ring* has finished hearing it.

The tricky part with a big crowd: several people in the current ring might all know the
same person one ring further out. If two of them try to tell that person "you just heard
the rumor!" at the same instant, that person must still only count as "newly told" once,
not twice, and not skipped because both tellers assumed the other would handle it.

## What you're actually building

```cpp
std::vector<int> parallel_bfs(const std::vector<std::vector<int>>& adj, int source,
int num_threads = 8);
```

`adj[u]` lists `u`'s neighbors. Return `dist`, where `dist[v]` is the shortest number of
edges from `source` to `v` (which ring they're in), or `-1` if `v` never hears the rumor
at all.

## Requirements

1. `dist` must exactly match sequential BFS, for any graph, including one where many
 nodes at the current level share neighbors in common.
2. **A node must never be processed as "newly discovered" more than once**, including
 when two different threads, working on two different current-level nodes, both reach
 the *same* next-level node at the same moment. The "you just heard it!" race, made
 precise.
3. Actually parallelizes each level's work across `num_threads` threads.

## Why the constraints exist

**`source` may have no outgoing edges, and the graph may be disconnected**, nodes
unreachable from `source` never hear the rumor and keep `dist = -1`.
