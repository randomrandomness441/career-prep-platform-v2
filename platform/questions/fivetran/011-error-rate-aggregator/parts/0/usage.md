Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`, which also defines `LogLine`):

```java
class LogLine {
    final long timestampMillis;
    final String level;
    final String message;
    LogLine(long timestampMillis, String level, String message) {
        this.timestampMillis = timestampMillis; this.level = level; this.message = message;
    }
}

class Solution {
    static LogLine parseLine(String raw) { ... }
    static double errorRate(List<LogLine> lines, long nowMillis, long windowMillis) { ... }
    static boolean isAlerting(List<LogLine> lines, long nowMillis, long windowMillis, double thresholdRatio) { ... }
}
```

Example:

```java
List<LogLine> lines = List.of(
    Solution.parseLine("0|ERROR|boom"),
    Solution.parseLine("70000|INFO|ok"));

Solution.errorRate(lines, 70000, 60000);       // 0.0 -- the ERROR at t=0 falls outside the
                                                // window, so only the INFO line at t=70000 counts
Solution.isAlerting(lines, 70000, 60000, 0.05); // false
```
