import java.util.*;

interface LineSource {
    String next();
}

enum MalformedPolicy { SKIP_AND_COUNT, FAIL_FAST }

class ParseResult {
    final List<Map<String, String>> records;
    final int malformedCount;
    ParseResult(List<Map<String, String>> records, int malformedCount) {
        this.records = records; this.malformedCount = malformedCount;
    }
}

class ArrayLineSource implements LineSource {
    private final List<String> lines;
    private int idx = 0;
    private boolean exhausted = false;
    int callCount = 0;
    ArrayLineSource(List<String> lines) { this.lines = lines; }
    public String next() {
        callCount++;
        if (exhausted) throw new AssertionError("next() called again after it already returned null");
        if (idx >= lines.size()) { exhausted = true; return null; }
        return lines.get(idx++);
    }
}

public class Tests {
    static int fails = 0;

    static void expect(boolean cond, String name) {
        if (!cond) { System.out.println("FAILED: " + name); fails++; }
    }

    public static void main(String[] args) {
        List<String> clean = List.of("a,b,c", "1,2,3", "4,5,6");
        ParseResult r1 = Solution.parse(new ArrayLineSource(clean), MalformedPolicy.SKIP_AND_COUNT);
        expect(r1.records.size() == 2, "clean input: 2 data rows parsed, got " + r1.records.size());
        expect(r1.malformedCount == 0, "clean input: no malformed rows");
        expect("1".equals(r1.records.get(0).get("a")) && "3".equals(r1.records.get(0).get("c")),
            "clean input: fields mapped to the right header column");

        // Malformed row in the middle, SKIP_AND_COUNT: skip it, keep parsing what follows.
        List<String> withBad = List.of("a,b,c", "1,2,3", "7,8", "9,10,11");
        ParseResult r2 = Solution.parse(new ArrayLineSource(withBad), MalformedPolicy.SKIP_AND_COUNT);
        expect(r2.malformedCount == 1, "SKIP_AND_COUNT: exactly one malformed row counted, got " + r2.malformedCount);
        expect(r2.records.size() == 2, "SKIP_AND_COUNT: both well-formed rows parsed, got " + r2.records.size());
        expect("9".equals(r2.records.get(1).get("a")),
            "SKIP_AND_COUNT: parsing continues past the malformed row instead of stopping");

        // Same input, FAIL_FAST: throws on the malformed row, doesn't read past it.
        ArrayLineSource src3 = new ArrayLineSource(withBad);
        boolean threw = false;
        try {
            Solution.parse(src3, MalformedPolicy.FAIL_FAST);
        } catch (IllegalStateException expected) {
            threw = true;
        }
        expect(threw, "FAIL_FAST: throws IllegalStateException on the first malformed row");
        expect(src3.callCount == 3,
            "FAIL_FAST: stops reading immediately, doesn't call next() past the malformed row, got " + src3.callCount + " calls");

        // Header-only input: no data rows, no crash.
        ParseResult r4 = Solution.parse(new ArrayLineSource(List.of("a,b,c")), MalformedPolicy.SKIP_AND_COUNT);
        expect(r4.records.isEmpty() && r4.malformedCount == 0, "header-only input produces no rows and no malformed count");

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
