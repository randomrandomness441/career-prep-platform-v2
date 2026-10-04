import java.util.*;

class Solution {
    static LogLine parseLine(String raw) {
        String[] parts = raw.split("\\|", 3);
        if (parts.length != 3) {
            throw new IllegalArgumentException("malformed log line: " + raw);
        }
        return new LogLine(Long.parseLong(parts[0]), parts[1], parts[2]);
    }

    static double errorRate(List<LogLine> lines, long nowMillis, long windowMillis) {
        long windowStart = nowMillis - windowMillis;
        int errors = 0;
        for (LogLine line : lines) {
            if (line.timestampMillis > windowStart && line.timestampMillis <= nowMillis
                    && "ERROR".equals(line.level)) {
                errors++;
            }
        }
        // TODO: divides by the total number of lines ever passed in, not just the ones
        // inside the window. An old error burst from hours ago is still in `lines` and
        // keeps diluting the denominator forever, so a currently-clean window can report
        // a rate that's nowhere near what's actually happening right now.
        return lines.isEmpty() ? 0.0 : (double) errors / lines.size();
    }

    static boolean isAlerting(List<LogLine> lines, long nowMillis, long windowMillis, double thresholdRatio) {
        return errorRate(lines, nowMillis, windowMillis) > thresholdRatio;
    }
}
