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
        int total = 0, errors = 0;
        for (LogLine line : lines) {
            if (line.timestampMillis > windowStart && line.timestampMillis <= nowMillis) {
                total++;
                if ("ERROR".equals(line.level)) errors++;
            }
        }
        return total == 0 ? 0.0 : (double) errors / total;
    }

    static boolean isAlerting(List<LogLine> lines, long nowMillis, long windowMillis, double thresholdRatio) {
        return errorRate(lines, nowMillis, windowMillis) > thresholdRatio;
    }
}
