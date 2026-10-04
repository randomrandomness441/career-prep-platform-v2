import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

class Job {
    final String id;
    final int priority;
    Job(String id, int priority) { this.id = id; this.priority = priority; }
}

public class Tests {
    static int fails = 0;

    static void expect(boolean cond, String name) {
        if (!cond) { System.out.println("FAILED: " + name); fails++; }
    }

    public static void main(String[] args) throws Exception {
        // --- priority ordering ---
        Solution pq = new Solution(3, 100);
        pq.submit(new Job("low", 1));
        pq.submit(new Job("high", 10));
        pq.submit(new Job("mid", 5));
        expect("high".equals(pq.poll(0).id), "priority: highest priority polled first");
        expect("mid".equals(pq.poll(0).id), "priority: then the next highest");
        expect("low".equals(pq.poll(0).id), "priority: lowest last");
        expect(pq.poll(0) == null, "priority: empty queue returns null, doesn't block");

        // --- equal priority: submission order (FIFO) breaks ties ---
        Solution fifo = new Solution(3, 100);
        fifo.submit(new Job("a", 5));
        fifo.submit(new Job("b", 5));
        fifo.submit(new Job("c", 5));
        expect("a".equals(fifo.poll(0).id), "tie-break: first submitted, first out");
        expect("b".equals(fifo.poll(0).id), "tie-break: second");
        expect("c".equals(fifo.poll(0).id), "tie-break: third");

        // --- retry backoff: not ready again until the backoff window has passed ---
        Solution retry = new Solution(5, 100);
        Job job = new Job("flaky", 0);
        retry.submit(job);
        Job polled1 = retry.poll(0);
        expect(polled1 != null, "retry: job is ready and polled at t=0");
        retry.fail(polled1, 0);                      // attempt 1 failed -> readyAt = 0 + 100*2^0 = 100
        expect(retry.poll(50) == null, "retry: not ready again before the backoff window (t=50 < 100)");
        Job polled2 = retry.poll(100);
        expect(polled2 != null, "retry: ready again once the backoff window elapses (t=100)");
        retry.fail(polled2, 100);                     // attempt 2 failed -> readyAt = 100 + 100*2^1 = 300
        expect(retry.poll(250) == null, "retry: backoff doubles on the second failure (not ready at t=250)");
        expect(retry.poll(300) != null, "retry: ready again at t=300");

        // --- max attempts: dropped for good after the last allowed failure ---
        Solution capped = new Solution(2, 10);
        Job cappedJob = new Job("doomed", 0);
        capped.submit(cappedJob);
        Job p1 = capped.poll(0);
        capped.fail(p1, 0);                 // attempt 1 of 2 -> requeued, readyAt = 10
        Job p2 = capped.poll(10);
        expect(p2 != null, "max attempts: still gets its second attempt");
        capped.fail(p2, 10);                // attempt 2 of 2 -> dropped, not requeued
        expect(capped.poll(1_000_000) == null, "max attempts: dropped for good once attempts are exhausted");

        // --- concurrency: producers and consumers running at the same time. Every
        // submitted job is delivered exactly once -- no loss, no double delivery. ---
        final int producers = 8, jobsPerProducer = 200, consumers = 8;
        final int total = producers * jobsPerProducer;
        Solution stress = new Solution(3, 100);
        ConcurrentHashMap<String, Boolean> seen = new ConcurrentHashMap<>();
        AtomicInteger delivered = new AtomicInteger(0);
        AtomicBoolean duplicate = new AtomicBoolean(false);

        ExecutorService exec = Executors.newFixedThreadPool(producers + consumers);
        List<Future<?>> futures = new ArrayList<>();

        for (int p = 0; p < producers; p++) {
            final int pid = p;
            futures.add(exec.submit(() -> {
                for (int i = 0; i < jobsPerProducer; i++) {
                    stress.submit(new Job("p" + pid + "-" + i, i % 5));
                }
            }));
        }
        for (int c = 0; c < consumers; c++) {
            futures.add(exec.submit(() -> {
                while (delivered.get() < total) {
                    Job j = stress.poll(Long.MAX_VALUE);
                    if (j == null) { Thread.onSpinWait(); continue; }
                    if (seen.putIfAbsent(j.id, Boolean.TRUE) != null) duplicate.set(true);
                    delivered.incrementAndGet();
                    stress.complete(j);
                }
            }));
        }
        for (Future<?> f : futures) f.get(60, TimeUnit.SECONDS);
        exec.shutdown();

        expect(!duplicate.get(), "concurrency: no job was ever delivered to two callers at once");
        expect(seen.size() == total, "concurrency: every submitted job was delivered exactly once, got " + seen.size() + "/" + total);

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
