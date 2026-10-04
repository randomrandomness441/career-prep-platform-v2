import java.util.*;

class Solution {
    private static class Entry {
        final Job job;
        int attempts;
        long readyAt;
        final long seq;
        Entry(Job job, int attempts, long readyAt, long seq) {
            this.job = job; this.attempts = attempts; this.readyAt = readyAt; this.seq = seq;
        }
    }

    private final int maxAttempts;
    private final long baseBackoffMillis;
    private final List<Entry> queue = new ArrayList<>();
    private final Map<Job, Entry> inFlight = new IdentityHashMap<>();
    private long seqCounter = 0;

    Solution(int maxAttempts, long baseBackoffMillis) {
        this.maxAttempts = maxAttempts;
        this.baseBackoffMillis = baseBackoffMillis;
    }

    synchronized void submit(Job job) {
        queue.add(new Entry(job, 0, Long.MIN_VALUE, seqCounter++));
    }

    synchronized Job poll(long nowMillis) {
        Entry best = null;
        for (Entry e : queue) {
            if (e.readyAt > nowMillis) continue;
            if (best == null
                    || e.job.priority > best.job.priority
                    || (e.job.priority == best.job.priority && e.seq < best.seq)) {
                best = e;
            }
        }
        if (best == null) return null;
        queue.remove(best);
        inFlight.put(best.job, best);
        return best.job;
    }

    synchronized void fail(Job job, long nowMillis) {
        Entry e = inFlight.remove(job);
        e.attempts++;
        if (e.attempts >= maxAttempts) return; // dead-lettered, doesn't come back
        long delay = baseBackoffMillis * (1L << (e.attempts - 1));
        e.readyAt = nowMillis + delay;
        queue.add(e);
    }

    synchronized void complete(Job job) {
        inFlight.remove(job);
    }
}
