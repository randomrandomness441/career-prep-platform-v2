Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`, which also defines these types):

```java
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
    final int status;                 // 200 = ok, 429 = rate limited
    final Integer retryAfterSeconds;  // set only on a 429, may still be null
    ApiResponse(int status, Integer retryAfterSeconds) { ... }
}

interface RateLimitedClient {
    ApiResponse call();
}

interface Sleeper {
    void sleep(long millis);
}

class Solution {
    static boolean tryAcquire(TokenBucket bucket, long nowMillis) { ... }
    static ApiResponse callWithBackoff(RateLimitedClient client, Sleeper sleeper, Random rng,
                                        int maxAttempts, long baseDelayMillis, long maxDelayMillis) { ... }
}
```

A caller pairs the two: check the bucket before every outbound call, and wrap the call
itself in the backoff helper for the case a request still gets rate limited.

```java
TokenBucket bucket = new TokenBucket(10, 5, System.currentTimeMillis());
for (String item : items) {
    while (!Solution.tryAcquire(bucket, System.currentTimeMillis())) {
        Thread.sleep(50); // real code waits for the next token
    }
    ApiResponse r = Solution.callWithBackoff(() -> fetch(item),
                                              ms -> { try { Thread.sleep(ms); } catch (InterruptedException ignored) {} },
                                              new Random(), 5, 200, 5000);
}
```
