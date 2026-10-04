A connector calls a source API that enforces a rate limit and returns HTTP 429 when
you go over it. You need two pieces of logic: a token bucket to self-throttle outbound
calls, and a retry loop that backs off on a 429 instead of hammering the API again
immediately.

Implement both, as static methods on `Solution`:

    static boolean tryAcquire(TokenBucket bucket, long nowMillis)

    static ApiResponse callWithBackoff(RateLimitedClient client, Sleeper sleeper, Random rng,
                                        int maxAttempts, long baseDelayMillis, long maxDelayMillis)

`TokenBucket` holds `capacity`, `refillPerSecond`, current `tokens`, and `lastRefillMillis`
(all given, see Usage). `tryAcquire` refills the bucket based on elapsed time since
`lastRefillMillis`, clamps at `capacity`, then consumes one token if at least one is
available. Return whether a token was consumed, and update the bucket's `tokens` and
`lastRefillMillis` in place either way.

`callWithBackoff` calls `client.call()`, retrying on a 429 response up to `maxAttempts`
total calls:

- If the 429 response carries a `retryAfterSeconds`, wait exactly that many seconds
  (as millis) via `sleeper.sleep(...)` before the next attempt.
- Otherwise wait `min(baseDelayMillis * 2^(attempt-1), maxDelayMillis)` milliseconds of
  exponential backoff, but sleep only a **jittered** amount: a random value between 0 and
  that capped delay, inclusive, drawn from `rng`.
- Return the first non-429 response.
- If every one of `maxAttempts` calls comes back 429, throw `IllegalStateException`
  instead of retrying forever.

**Requirements**

- `tryAcquire` never lets `tokens` exceed `capacity`, even after a long idle gap.
- The exponential delay is always capped at `maxDelayMillis`, no matter how many retries
  have happened.
- A `retryAfterSeconds` on the response always wins over the computed backoff -- don't
  compute or apply jitter when the server told you exactly how long to wait.
- `sleeper.sleep(...)` is the only place a wait happens -- never call `Thread.sleep`
  directly (the test harness doesn't want to actually wait).
