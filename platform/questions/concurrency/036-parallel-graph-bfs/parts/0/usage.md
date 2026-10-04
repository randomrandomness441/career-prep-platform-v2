### How it's called

```cpp
std::vector<std::vector<int>> adj = {
    {1, 2},    // node 0's neighbors
    {0, 3},    // node 1's neighbors
    {0, 3},    // node 2's neighbors
    {1, 2},    // node 3's neighbors
    /* node 4: unreachable from 0 */ {}
};

std::vector<int> dist = parallel_bfs(adj, /*source=*/0, /*num_threads=*/8);
// dist == {0, 1, 1, 2, -1}
```

A single call, internally spreading each level's neighbor-discovery work across
`num_threads` threads it creates and joins itself, one level at a time.
