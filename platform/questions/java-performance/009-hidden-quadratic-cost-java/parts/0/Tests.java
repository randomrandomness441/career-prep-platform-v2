import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class Tests {
    static int fails = 0;

    static void expectEq(List<Integer> got, List<Integer> want, String name) {
        boolean ok = got.size() == want.size();
        for (int i = 0; ok && i < got.size(); i++) ok = got.get(i).equals(want.get(i));
        if (!ok) {
            System.out.printf("%s: wrong output (size got=%d want=%d)%n", name, got.size(), want.size());
            fails++;
        }
    }

    public static void main(String[] args) {
        expectEq(Solution.dedupe(List.of()), List.of(), "empty input");
        expectEq(Solution.dedupe(List.of(1, 2, 3)), List.of(1, 2, 3), "no duplicates");
        expectEq(Solution.dedupe(List.of(5, 5, 5, 5)), List.of(5), "all duplicates");
        expectEq(Solution.dedupe(List.of(3, 1, 3, 2, 1)), List.of(3, 1, 2),
                "duplicates keep first-occurrence order");

        // Large-scale correctness + a real time budget. Deterministic linear-congruential
        // sequence, not Random, so every run is identical -- no flakiness.
        final int N = 100000;
        List<Integer> ids = new ArrayList<>(N);
        long x = 12345;
        for (int i = 0; i < N; i++) {
            x = x * 1103515245L + 12345L;
            ids.add((int) Math.floorMod(x, N));
        }

        // Independent reference computation, not calling Solution.dedupe() itself.
        List<Integer> expected = new ArrayList<>(N);
        Set<Integer> seenRef = new HashSet<>();
        for (int id : ids) {
            if (seenRef.add(id)) expected.add(id);
        }

        long t0 = System.nanoTime();
        List<Integer> got = Solution.dedupe(ids);
        long t1 = System.nanoTime();
        double ms = (t1 - t0) / 1e6;

        expectEq(got, expected, "large input, correctness");

        // Measured on this machine: a correct O(n) HashSet-based version takes ~21ms
        // at this N. The naive O(n^2) version above takes ~1540ms. 400ms budget gives
        // huge margin on both sides without ever letting the quadratic version through.
        final double BUDGET_MS = 400.0;
        if (ms > BUDGET_MS) {
            System.out.printf(
                "dedupe(%d ids) took %.1fms, must be under %.0fms.%n" +
                "That gap is what async-profiler would have shown you before you ever%n" +
                "got here -- something in this function is doing far more work per%n" +
                "element than it looks like from reading it.%n", N, ms, BUDGET_MS);
            fails++;
        } else {
            System.out.printf("large input: correct and %.1fms (budget %.0fms)%n", ms, BUDGET_MS);
        }

        if (fails > 0) {
            System.out.printf("%d check(s) failed%n", fails);
            System.exit(1);
        }
        System.out.println("all checks passed");
    }
}
