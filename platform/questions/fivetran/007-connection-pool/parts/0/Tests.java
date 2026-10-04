import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

interface MockConnection {
    boolean isValid();
    void close();
}

interface ConnectionFactory {
    MockConnection create();
}

class FakeConnection implements MockConnection {
    volatile boolean valid = true;
    volatile boolean closed = false;
    public boolean isValid() { return valid; }
    public void close() { closed = true; }
}

class CountingFactory implements ConnectionFactory {
    final List<FakeConnection> created = Collections.synchronizedList(new ArrayList<>());
    public synchronized MockConnection create() {
        FakeConnection c = new FakeConnection();
        created.add(c);
        return c;
    }
}

public class Tests {
    static int fails = 0;

    static void expect(boolean cond, String name) {
        if (!cond) { System.out.println("FAILED: " + name); fails++; }
    }

    public static void main(String[] args) throws Exception {
        // --- A: single-threaded reuse and eviction ---
        CountingFactory fa = new CountingFactory();
        Solution poolA = new Solution(fa, 2);
        MockConnection c1 = poolA.lease();
        poolA.release(c1);
        MockConnection c2 = poolA.lease();
        expect(c2 == c1, "a valid released connection is reused, not recreated");

        ((FakeConnection) c2).valid = false;
        poolA.release(c2);
        expect(((FakeConnection) c2).closed, "an invalid connection gets close() called on release");

        MockConnection c3 = poolA.lease();
        expect(c3 != c2, "an evicted connection is never handed out again");
        expect(fa.created.size() == 2, "eviction caused exactly one replacement connection to be created, got " + fa.created.size());

        // --- B: lease blocks at capacity, unblocks on release ---
        CountingFactory fb = new CountingFactory();
        Solution poolB = new Solution(fb, 1);
        MockConnection only = poolB.lease();
        AtomicBoolean gotIt = new AtomicBoolean(false);
        Thread waiter = new Thread(() -> {
            try { poolB.lease(); gotIt.set(true); } catch (InterruptedException ignored) {}
        });
        waiter.start();
        Thread.sleep(150);
        expect(!gotIt.get(), "lease() blocks when the pool is full and nothing is idle");
        poolB.release(only);
        waiter.join(2000);
        expect(gotIt.get(), "lease() unblocks once release() makes room");

        // --- C: concurrency stress. No connection ever gets handed to two threads at
        // once, and the pool never creates more than maxSize connections total (no
        // evictions in this run, so that's a hard, deterministic bound, not a guess). ---
        final int maxSize = 4, threadCount = 16, itersPerThread = 400;
        CountingFactory fc = new CountingFactory();
        Solution poolC = new Solution(fc, maxSize);
        ConcurrentHashMap<MockConnection, Boolean> currentlyOut = new ConcurrentHashMap<>();
        AtomicBoolean doubleLease = new AtomicBoolean(false);
        ExecutorService exec = Executors.newFixedThreadPool(threadCount);
        List<Future<?>> futures = new ArrayList<>();
        for (int t = 0; t < threadCount; t++) {
            futures.add(exec.submit(() -> {
                try {
                    for (int i = 0; i < itersPerThread; i++) {
                        MockConnection c = poolC.lease();
                        if (currentlyOut.putIfAbsent(c, Boolean.TRUE) != null) {
                            doubleLease.set(true);
                        }
                        currentlyOut.remove(c);
                        poolC.release(c);
                    }
                } catch (InterruptedException ignored) {
                }
            }));
        }
        for (Future<?> f : futures) f.get(60, TimeUnit.SECONDS);
        exec.shutdown();

        expect(!doubleLease.get(), "no connection was ever leased to two threads at the same time");
        expect(fc.created.size() <= maxSize,
            "never creates more than maxSize=" + maxSize + " connections total, got " + fc.created.size());

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
