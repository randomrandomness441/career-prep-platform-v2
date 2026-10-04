A fleet of workers each own a share of the connectors that need to sync. Workers get added
and removed as the fleet scales, and every time that happens, `assign(key) = hash(key) %
numWorkers` reshuffles almost every connector to a different worker -- a one-worker change
you'd expect to be minor forces nearly the whole fleet to drop and reopen its connections.

Implement a class `Solution`, a hash ring:

    Solution(List<String> workers, int virtualNodesPerWorker)
    String assign(String key)
    void addWorker(String worker)
    void removeWorker(String worker)

Each worker gets `virtualNodesPerWorker` points placed on the ring (hash each of
`worker + "#" + i` for `i` in `0 until virtualNodesPerWorker`). `assign(key)` hashes `key`
and returns the worker owning the next point clockwise on the ring (wrapping around past
the largest point back to the smallest).

**Requirements**

- Removing a worker must change the assignment of only the keys that were assigned to
  that worker. Every other key's `assign(key)` result must come back byte-for-byte
  identical to what it was before the removal.
- Symmetrically, adding a worker must only pull keys onto its own new points -- it must
  never change which worker a key maps to unless that key's new owner is the worker just
  added.
- `virtualNodesPerWorker` exists so that with enough of them, each worker ends up owning
  roughly an even share of the ring, not a lucky or unlucky single point.
