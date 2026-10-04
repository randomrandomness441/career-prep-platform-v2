import java.util.*;

class Solution {
    private final List<String> workers;

    Solution(List<String> workers, int virtualNodesPerWorker) {
        this.workers = new ArrayList<>(workers);
    }

    // TODO: plain hash % numWorkers. It's simple and it works -- until the fleet size
    // changes. Removing or adding one worker shifts almost every key to a different
    // index, so nearly the whole fleet gets reassigned instead of just the keys that
    // were actually on the worker that changed.
    String assign(String key) {
        int idx = Math.floorMod(key.hashCode(), workers.size());
        return workers.get(idx);
    }

    void addWorker(String worker) {
        workers.add(worker);
    }

    void removeWorker(String worker) {
        workers.remove(worker);
    }
}
