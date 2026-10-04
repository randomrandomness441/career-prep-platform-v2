import java.util.*;

public class Tests {

    public static void main(String[] args) {
        int fails = 0;

        fails += check(new int[]{4, 2, 1, 3},
                new int[][]{{1, 2}, {2, 3}, {3, 4}}, "example1");

        fails += check(new int[]{1, 3, 6, 10, 15},
                new int[][]{{1, 3}}, "example2");

        fails += check(new int[]{3, 8, -10, 23, 19, -4, -14, 27},
                new int[][]{{-14, -10}, {19, 23}, {23, 27}}, "example3");

        fails += check(new int[]{1, 2},
                new int[][]{{1, 2}}, "minLength");

        fails += check(new int[]{1, 10, 11},
                new int[][]{{10, 11}}, "minDiffAppearsLate");

        fails += check(new int[]{-1000000, 1000000},
                new int[][]{{-1000000, 1000000}}, "boundaryValues");

        fails += check(new int[]{-5, -1, -3},
                new int[][]{{-5, -3}, {-3, -1}}, "allNegative");

        fails += check(new int[]{5, 4, 3, 2, 1},
                new int[][]{{1, 2}, {2, 3}, {3, 4}, {4, 5}}, "reverseConsecutive");

        fails += check(new int[]{40, 20, 31, 45},
                new int[][]{{40, 45}}, "shrinkingGaps");

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    private static int check(int[] input, int[][] expected, String name) {
        List<List<Integer>> got = new Solution().minimumAbsDifference(input);
        List<List<Integer>> exp = new ArrayList<>();
        for (int[] pair : expected) {
            List<Integer> p = new ArrayList<>();
            p.add(pair[0]);
            p.add(pair[1]);
            exp.add(p);
        }
        if (!pairsEqual(got, exp)) {
            System.out.println("FAIL " + name + ": expected " + exp + " but got " + got);
            return 1;
        }
        return 0;
    }

    private static boolean pairsEqual(List<List<Integer>> a, List<List<Integer>> b) {
        if (a == null || b == null) return a == b;
        if (a.size() != b.size()) return false;
        for (int i = 0; i < a.size(); i++) {
            if (!a.get(i).get(0).equals(b.get(i).get(0))) return false;
            if (!a.get(i).get(1).equals(b.get(i).get(1))) return false;
        }
        return true;
    }
}