import java.util.*;

class Solution {
    private final ConnectionFactory factory;
    private final int maxSize;
    private final Deque<MockConnection> idle = new ArrayDeque<>();
    private int created = 0;

    Solution(ConnectionFactory factory, int maxSize) {
        this.factory = factory; this.maxSize = maxSize;
    }

    // TODO: "check idle, else create if under max" is not one atomic step, and the idle
    // deque plus the created counter aren't protected by any lock. Two threads can both
    // see the deque empty and both see created < maxSize, and both create a new
    // connection -- or both poll() the same idle connection out from under each other.
    // Looks fine single-threaded; falls apart under real concurrent lease/release.
    MockConnection lease() throws InterruptedException {
        MockConnection c = idle.poll();
        if (c != null) {
            if (c.isValid()) return c;
            c.close();
            created--;
        }
        while (created >= maxSize) {
            Thread.sleep(1);
        }
        created++;
        return factory.create();
    }

    void release(MockConnection conn) {
        if (conn.isValid()) {
            idle.offer(conn);
        } else {
            conn.close();
            created--;
        }
    }
}
