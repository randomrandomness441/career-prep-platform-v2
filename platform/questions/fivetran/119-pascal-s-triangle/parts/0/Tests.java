import java.util.Arrays;
import java.util.List;

public class Tests {

    static int fails = 0;

    public static void main(String[] args) {
        Solution s = new Solution();

        // 1. smallest allowed input
        check(s.generate(1), Arrays.asList(
                Arrays.asList(1)
        ), "numRows=1 should give [[1]]");

        // 2. two rows
        check(s.generate(2), Arrays.asList(
                Arrays.asList(1),
                Arrays.asList(1, 1)
        ), "numRows=2 should give [[1],[1,1]]");

        // 3. three rows
        check(s.generate(3), Arrays.asList(
                Arrays.asList(1),
                Arrays.asList(1, 1),
                Arrays.asList(1, 2, 1)
        ), "numRows=3 should give [[1],[1,1],[1,2,1]]");

        // 4. LeetCode example
        check(s.generate(5), Arrays.asList(
                Arrays.asList(1),
                Arrays.asList(1, 1),
                Arrays.asList(1, 2, 1),
                Arrays.asList(1, 3, 3, 1),
                Arrays.asList(1, 4, 6, 4, 1)
        ), "numRows=5 LeetCode example");

        // 5. upper bound of the constraints: numRows = 30
        List<List<Integer>> big = s.generate(30);
        if (big.size() != 30) {
            fail("numRows=30 should return 30 rows, got " + big.size());
        }
        if (!big.isEmpty()) {
            List<Integer> last = big.get(big.size() - 1);
            if (last.size() != 30) {
                fail("last row should have 30 entries, got " + last.size());
            } else {
                if (last.get(0) != 1 || last.get(29) != 1) {
                    fail("last row edges should both be 1");
                }
                if (last.get(14) != 77558760) {
                    fail("last row middle entry should be 77558760, got " + last.get(14));
                }
            }
        }

        // 6. shape of a mid-size triangle: numRows = 10
        List<List<Integer>> t10 = s.generate(10);
        boolean shapeOk = t10.size() == 10;
        for (int i = 0; shapeOk && i < t10.size(); i++) {
            List<Integer> row = t10.get(i);
            if (row.size() != i + 1) {
                shapeOk = false;
            } else if (row.get(0) != 1 || row.get(row.size() - 1) != 1) {
                shapeOk = false;
            }
        }
        if (!shapeOk) {
            fail("numRows=10 every row i should have i+1 entries and 1s on both edges");
        }

        // 7. row sums double: row i sums to 2^i, checked at numRows = 12
        List<List<Integer>> t12 = s.generate(12);
        boolean sumsOk = t12.size() == 12;
        for (int i = 0; sumsOk && i < t12.size(); i++) {
            long sum = 0;
            for (int v : t12.get(i)) {
                sum += v;
            }
            if (sum != (1L << i)) {
                sumsOk = false;
            }
        }
        if (!sumsOk) {
            fail("numRows=12 row i should sum to 2^i");
        }

        // 8. Pascal recurrence on interior cells, checked at numRows = 6
        List<List<Integer>> t6 = s.generate(6);
        boolean recOk = t6.size() == 6;
        for (int i = 1; recOk && i < t6.size(); i++) {
            List<Integer> above = t6.get(i - 1);
            List<Integer> row = t6.get(i);
            for (int j = 1; recOk && j < row.size() - 1; j++) {
                if (row.get(j) != above.get(j - 1) + above.get(j)) {
                    recOk = false;
                }
            }
        }
        if (!recOk) {
            fail("numRows=6 interior values should equal the sum of the two above");
        }

        // 9. calling again should give a fresh, correct triangle
        if (!s.generate(1).equals(Arrays.asList(Arrays.asList(1)))) {
            fail("repeat call with numRows=1 should still return [[1]]");
        }

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static void check(List<List<Integer>> actual, List<List<Integer>> expected, String name) {
        if (!actual.equals(expected)) {
            fail(name + " expected " + expected + " but got " + actual);
        }
    }

    static void fail(String message) {
        fails++;
        System.out.println("FAILED: " + message);
    }
}