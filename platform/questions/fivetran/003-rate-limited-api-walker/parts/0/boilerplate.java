import java.util.*;

class Solution {
    static boolean tryAcquire(TokenBucket bucket, long nowMillis) {
        long elapsed = nowMillis - bucket.lastRefillMillis;
        if (elapsed > 0) {
            double refill = (elapsed / 1000.0) * bucket.refillPerSecond;
            bucket.tokens = Math.min(bucket.capacity, bucket.tokens + refill);
            bucket.lastRefillMillis = nowMillis;
        }
        if (bucket.tokens >= 1.0) {
            bucket.tokens -= 1.0;
            return true;
        }
        return false;
    }

    static ApiResponse callWithBackoff(RateLimitedClient client, Sleeper sleeper, Random rng,
                                        int maxAttempts, long baseDelayMillis, long maxDelayMillis) {
        for (int attempt = 1; attempt <= maxAttempts; attempt++) {
            ApiResponse resp = client.call();
            if (resp.status != 429) return resp;
            if (attempt == maxAttempts) {
                throw new IllegalStateException("rate limited after " + maxAttempts + " attempts");
            }
            // TODO: always computes exponential backoff, even when the response gave us a
            // Retry-After. A server telling us "wait exactly 30s" deserves better than us
            // guessing with a formula -- and ignoring it can mean we retry too soon and get
            // rate limited again, or wait longer than necessary.
            long uncapped = baseDelayMillis * (1L << (attempt - 1));
            long capped = Math.min(uncapped, maxDelayMillis);
            long delay = rng.nextInt((int) capped + 1);
            sleeper.sleep(delay);
        }
        throw new IllegalStateException("unreachable");
    }
}
