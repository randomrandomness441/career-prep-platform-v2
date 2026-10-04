import java.util.*;

public class Tests {

    public static void main(String[] args) {
        int fails = 0;

        // official example 1
        fails += check(new int[]{10, 1, 2, 7, 6, 1, 5}, 8,
                new int[][]{{1, 1, 6}, {1, 2, 5}, {1, 7}, {2, 6}});

        // official example 2
        fails += check(new int[]{2, 5, 2, 1, 2}, 5,
                new int[][]{{1, 2, 2}, {5}});

        // trivial: one candidate that exactly matches
        fails += check(new int[]{1}, 1, new int[][]{{1}});

        // target smaller than every candidate
        fails += check(new int[]{2}, 1, new int[][]{});

        // every candidate overshoots the target
        fails += check(new int[]{50, 50, 50}, 30, new int[][]{});

        // heavy duplicates: only one distinct combination exists
        fails += check(new int[]{1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 5,
                new int[][]{{1, 1, 1, 1, 1}});

        // duplicates plus a two-element answer
        fails += check(new int[]{3, 1, 3, 5, 1, 1}, 8,
                new int[][]{{1, 1, 1, 5}, {1, 1, 3, 3}, {3, 5}});

        // whole array sums to the target
        fails += check(new int[]{1, 2, 3}, 6, new int[][]{{1, 2, 3}});

        // max allowed target hit by a single candidate
        fails += check(new int[]{30}, 30, new int[][]{{30}});

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    private static int check(int[] candidates, int target, int[][] expectedRaw) {
        Solution s = new Solution();
        List<List<Integer>> actual = canonical(s.combinationSum2(candidates.clone(), target));
        List<List<Integer>> expected = canonical(toLists(expectedRaw));
        if (!expected.equals(actual)) {
            System.out.println("FAIL candidates=" + Arrays.toString(candidates)
                    + " target=" + target);
            System.out.println("  expected: " + expected);
            System.out.println("  actual:   " + actual);
            return 1;
        }
        return 0;
    }

    private static List<List<Integer>> toLists(int[][] arr) {
        List<List<Integer>> out = new ArrayList<>();
        for (int[] row : arr) {
            List<Integer> list = new ArrayList<>();
            for (int v : row) {
                list.add(v);
            }
            out.add(list);
        }
        return out;
    }

    // Sort each combination, then sort the outer list. Output order and inner
    // order never matter, but duplicate combinations do count against you.
    private static List<List<Integer>> canonical(List<List<Integer>> lists) {
        List<List<Integer>> copy = new ArrayList<>();
        for (List<Integer> list : lists) {
            List<Integer> inner = new ArrayList<>(list);
            Collections.sort(inner);
            copy.add(inner);
        }
        copy.sort((a, b) -> {
            int n = Math.min(a.size(), b.size());
            for (int i = 0; i < n; i++) {
                int cmp = Integer.compare(a.get(i), b.get(i));
                if (cmp != 0) {
                    return cmp;
                }
            }
            return Integer.compare(a.size(), b.size());
        });
        return copy;
    }
}