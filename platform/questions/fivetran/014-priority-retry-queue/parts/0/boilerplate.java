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

    // TODO: none of these four methods are synchronized, and they all mutate the same
    // plain ArrayList/HashMap. Single-threaded, this looks completely correct. With real
    // concurrent producers and consumers, two threads can both win the "which job is
    // best" scan and walk off with the same job, or a concurrent add/remove can corrupt
    // the ArrayList outright.
    void submit(Job job) {
        queue.add(new Entry(job, 0, Long.MIN_VALUE, seqCounter++));
    }

    Job poll(long nowMillis) {
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

    void fail(Job job, long nowMillis) {
        Entry e = inFlight.remove(job);
        e.attempts++;
        if (e.attempts >= maxAttempts) return;
        long delay = baseBackoffMillis * (1L << (e.attempts - 1));
        e.readyAt = nowMillis + delay;
        queue.add(e);
    }

    void complete(Job job) {
        inFlight.remove(job);
    }
}
