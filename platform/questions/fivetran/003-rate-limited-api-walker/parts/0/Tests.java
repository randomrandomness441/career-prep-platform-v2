import java.util.*;

class TokenBucket {
    final double capacity;
    final double refillPerSecond;
    double tokens;
    long lastRefillMillis;
    TokenBucket(double capacity, double refillPerSecond, long nowMillis) {
        this.capacity = capacity; this.refillPerSecond = refillPerSecond;
        this.tokens = capacity; this.lastRefillMillis = nowMillis;
    }
}

class ApiResponse {
    final int status;
    final Integer retryAfterSeconds;
    ApiResponse(int status, Integer retryAfterSeconds) {
        this.status = status; this.retryAfterSeconds = retryAfterSeconds;
    }
}

interface RateLimitedClient {
    ApiResponse call();
}

interface Sleeper {
    void sleep(long millis);
}

class SpySleeper implements Sleeper {
    List<Long> history = new ArrayList<>();
    public void sleep(long millis) { history.add(millis); }
}

// Returns responses from a fixed script, in order; repeats the last one once the
// script runs out, so an "always rate limited" test doesn't need a huge list.
class ScriptedClient implements RateLimitedClient {
    private final List<ApiResponse> script;
    private int idx = 0;
    int callCount = 0;
    ScriptedClient(List<ApiResponse> script) { this.script = script; }
    public ApiResponse call() {
        callCount++;
        ApiResponse r = script.get(Math.min(idx, script.size() - 1));
        idx++;
        return r;
    }
}

public class Tests {
    static int fails = 0;

    static void expect(boolean cond, String name) {
        if (!cond) { System.out.println("FAILED: " + name); fails++; }
    }

    public static void main(String[] args) {
        // --- tryAcquire: basic drain and clamp ---
        TokenBucket b1 = new TokenBucket(5, 1, 0);
        int acquired = 0;
        for (int i = 0; i < 6; i++) if (Solution.tryAcquire(b1, 0)) acquired++;
        expect(acquired == 5, "tryAcquire: bucket starts full at capacity, drains to empty, got " + acquired);
        expect(!Solution.tryAcquire(b1, 0), "tryAcquire: empty bucket at same instant stays empty");
        // 2 seconds later, refillPerSecond=1 -> 2 tokens back.
        int refilled = 0;
        for (int i = 0; i < 3; i++) if (Solution.tryAcquire(b1, 2000)) refilled++;
        expect(refilled == 2, "tryAcquire: refill is elapsed_seconds * refillPerSecond, got " + refilled);

        TokenBucket b2 = new TokenBucket(3, 10, 0);
        Solution.tryAcquire(b2, 0); // tokens: 3 -> 2
        // A long idle gap should clamp at capacity, not overflow past it.
        Solution.tryAcquire(b2, 5000); // would refill 50 tokens uncapped
        int afterLongGap = 0;
        for (int i = 0; i < 5; i++) if (Solution.tryAcquire(b2, 5000)) afterLongGap++;
        expect(afterLongGap <= 2, "tryAcquire: refill clamps at capacity even after a long idle gap, got " + afterLongGap + " extra tokens");

        // --- callWithBackoff: a Retry-After header wins over computed backoff ---
        ScriptedClient retryAfterClient = new ScriptedClient(List.of(
            new ApiResponse(429, 2), new ApiResponse(200, null)));
        SpySleeper s1 = new SpySleeper();
        ApiResponse r1 = Solution.callWithBackoff(retryAfterClient, s1, new Random(1), 5, 100, 5000);
        expect(r1.status == 200, "callWithBackoff: returns the first non-429 response");
        expect(s1.history.equals(List.of(2000L)),
            "callWithBackoff: Retry-After of 2s becomes a single 2000ms sleep, not exponential backoff, got " + s1.history);

        // --- callWithBackoff: exponential backoff with jitter, no Retry-After ---
        ScriptedClient backoffClient = new ScriptedClient(List.of(
            new ApiResponse(429, null), new ApiResponse(429, null),
            new ApiResponse(429, null), new ApiResponse(200, null)));
        SpySleeper s2 = new SpySleeper();
        Solution.callWithBackoff(backoffClient, s2, new Random(7), 10, 100, 1000);
        expect(s2.history.size() == 3, "callWithBackoff: one sleep per 429 before the success, got " + s2.history.size());
        long[] caps = {100, 200, 400};
        for (int i = 0; i < 3; i++) {
            long v = s2.history.get(i);
            expect(v >= 0 && v <= caps[i],
                "callWithBackoff: jittered sleep " + i + " must be in [0," + caps[i] + "], got " + v);
        }

        // --- callWithBackoff: the exponential delay is capped, never grows past maxDelayMillis ---
        List<ApiResponse> manyRetries = new ArrayList<>();
        for (int i = 0; i < 6; i++) manyRetries.add(new ApiResponse(429, null));
        manyRetries.add(new ApiResponse(200, null));
        SpySleeper s3 = new SpySleeper();
        Solution.callWithBackoff(new ScriptedClient(manyRetries), s3, new Random(99), 10, 100, 300);
        for (long v : s3.history) {
            expect(v <= 300, "callWithBackoff: sleep must never exceed maxDelayMillis=300, got " + v);
        }

        // --- callWithBackoff: exhausting every attempt throws instead of retrying forever ---
        ScriptedClient alwaysLimited = new ScriptedClient(List.of(new ApiResponse(429, null)));
        SpySleeper s4 = new SpySleeper();
        boolean threw = false;
        try {
            Solution.callWithBackoff(alwaysLimited, s4, new Random(3), 3, 50, 500);
        } catch (IllegalStateException expected) {
            threw = true;
        }
        expect(threw, "callWithBackoff: throws IllegalStateException once maxAttempts is exhausted");
        expect(alwaysLimited.callCount == 3, "callWithBackoff: makes exactly maxAttempts calls, got " + alwaysLimited.callCount);
        expect(s4.history.size() == 2, "callWithBackoff: sleeps between attempts but not after the final failed one, got " + s4.history.size());

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
