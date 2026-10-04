import java.util.*;

class LogLine {
    final long timestampMillis;
    final String level;
    final String message;
    LogLine(long timestampMillis, String level, String message) {
        this.timestampMillis = timestampMillis; this.level = level; this.message = message;
    }
}

public class Tests {
    static int fails = 0;

    static void expect(boolean cond, String name) {
        if (!cond) { System.out.println("FAILED: " + name); fails++; }
    }

    static void expectClose(double got, double want, String name) {
        if (Math.abs(got - want) > 1e-9) {
            System.out.printf("FAILED %s: got %s want %s%n", name, got, want);
            fails++;
        }
    }

    public static void main(String[] args) {
        // parseLine: well-formed line.
        LogLine parsed = Solution.parseLine("1000|INFO|hello world");
        expect(parsed.timestampMillis == 1000 && "INFO".equals(parsed.level) && "hello world".equals(parsed.message),
            "parseLine: splits timestamp, level, message correctly");

        // parseLine: malformed line (missing a segment) throws.
        boolean threw = false;
        try {
            Solution.parseLine("not-a-valid-line");
        } catch (IllegalArgumentException expected) {
            threw = true;
        }
        expect(threw, "parseLine: throws IllegalArgumentException on a malformed line");

        // errorRate: denominator is lines INSIDE the window, not every line ever seen.
        List<LogLine> lines = List.of(
            new LogLine(0, "ERROR", "old"),
            new LogLine(10000, "INFO", "boundary"),
            new LogLine(20000, "ERROR", "in window"),
            new LogLine(70000, "INFO", "now"));
        // window = (10000, 70000]: excludes t=0 and t=10000, includes t=20000 and t=70000.
        expectClose(Solution.errorRate(lines, 70000, 60000), 0.5,
            "errorRate: 1 error out of 2 lines actually inside the window, ignoring the older error outside it");

        // A clean recent window, despite an old burst of errors further back, must not
        // be dragged down by lines outside the window.
        List<LogLine> oldBurst = List.of(
            new LogLine(0, "ERROR", "a"), new LogLine(1000, "ERROR", "b"),
            new LogLine(2000, "ERROR", "c"), new LogLine(100000, "INFO", "clean"));
        expectClose(Solution.errorRate(oldBurst, 100000, 60000), 0.0,
            "errorRate: an old error burst outside the window doesn't affect a clean window's rate");

        // No lines at all inside the window -> 0.0, not NaN or a crash.
        expectClose(Solution.errorRate(List.of(new LogLine(0, "ERROR", "x")), 1_000_000, 1000), 0.0,
            "errorRate: an empty window returns 0.0");

        // isAlerting: strictly greater than the threshold.
        expect(Solution.isAlerting(lines, 70000, 60000, 0.4), "isAlerting: 0.5 > 0.4 fires");
        expect(!Solution.isAlerting(lines, 70000, 60000, 0.6), "isAlerting: 0.5 > 0.6 is false, doesn't fire");
        expect(!Solution.isAlerting(lines, 70000, 60000, 0.5), "isAlerting: exactly at the threshold doesn't fire (strictly greater)");

        // Unsorted input still computes correctly.
        List<LogLine> shuffled = List.of(
            new LogLine(70000, "INFO", "now"), new LogLine(0, "ERROR", "old"),
            new LogLine(20000, "ERROR", "in window"), new LogLine(10000, "INFO", "boundary"));
        expectClose(Solution.errorRate(shuffled, 70000, 60000), 0.5, "errorRate: works on unsorted input");

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
