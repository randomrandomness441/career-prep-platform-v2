import java.util.concurrent.*;

class Solution {
    private final ConnectionFactory factory;
    private final Semaphore permits;
    private final BlockingQueue<MockConnection> idle;

    Solution(ConnectionFactory factory, int maxSize) {
        this.factory = factory;
        this.permits = new Semaphore(maxSize, true);
        this.idle = new LinkedBlockingQueue<>();
    }

    MockConnection lease() throws InterruptedException {
        permits.acquire();
        while (true) {
            MockConnection c = idle.poll();
            if (c == null) {
                // Holding a permit is what makes this safe -- at most maxSize permits
                // exist, so at most maxSize connections can ever be created and
                // outstanding at once.
                return factory.create();
            }
            if (c.isValid()) return c;
            c.close();
            // Still holding the permit -- try another idle connection, or create fresh.
        }
    }

    void release(MockConnection conn) {
        if (conn.isValid()) {
            idle.offer(conn);
        } else {
            conn.close();
        }
        permits.release();
    }
}
