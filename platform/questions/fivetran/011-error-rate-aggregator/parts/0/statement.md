A connector's log lines look like `1700000000000|ERROR|connection refused`: timestamp in
epoch millis, level, message, pipe-delimited. An alert should fire when the error rate over
the trailing window gets too high -- not when a single old error happens to still be in the
log.

Implement three static methods on `Solution`:

    static LogLine parseLine(String raw)

    static double errorRate(List<LogLine> lines, long nowMillis, long windowMillis)

    static boolean isAlerting(List<LogLine> lines, long nowMillis, long windowMillis, double thresholdRatio)

`parseLine` splits `raw` into timestamp, level, and message on `|` and returns a `LogLine`.
Throw `IllegalArgumentException` if the line doesn't have all three parts.

`errorRate` computes `(# lines with level "ERROR") / (# lines total)` restricted to the
window `(nowMillis - windowMillis, nowMillis]` -- strictly after the window start,
up to and including `nowMillis`. Return `0.0` if no lines fall in that window at all (not
`NaN`, not a divide-by-zero crash).

`isAlerting` is true when `errorRate(...)` is strictly greater than `thresholdRatio`.

**Requirements**

- The denominator in `errorRate` is the count of lines *inside the window*, not every
  line ever passed in -- an old burst of errors outside the window must not affect a
  clean window's rate at all.
- `lines` isn't guaranteed sorted by timestamp.
