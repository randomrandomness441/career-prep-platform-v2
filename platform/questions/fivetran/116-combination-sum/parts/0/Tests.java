import java.util.*;

public class Tests {
    static int fails = 0;

    public static void main(String[] args) {
        check(new int[]{2, 3, 6, 7}, 7, new int[][]{{2, 2, 3}, {7}}, "example 1");
        check(new int[]{2, 3, 5}, 8, new int[][]{{2, 2, 2, 2}, {2, 3, 3}, {3, 5}}, "example 2");
        check(new int[]{2}, 1, new int[][]{}, "target below smallest candidate");
        check(new int[]{2}, 2, new int[][]{{2}}, "single candidate equals target");
        check(new int[]{7}, 14, new int[][]{{7, 7}}, "reuse same candidate twice");
        check(new int[]{6, 3, 7, 2}, 7, new int[][]{{2, 2, 3}, {7}}, "unsorted candidates");
        check(new int[]{3, 8, 12}, 11, new int[][]{{3, 8}}, "candidate larger than target");
        check(new int[]{2, 4}, 8, new int[][]{{2, 2, 2, 2}, {2, 2, 4}, {4, 4}}, "one candidate a multiple of another");

        if (fails > 0) {
            System.out.println(fails + " check(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static void check(int[] candidates, int target, int[][] expected, String label) {
        List<List<Integer>> got = new Solution().combinationSum(candidates, target);
        String gotKey = canonical(got);
        String wantKey = canonical(toLists(expected));
        if (!gotKey.equals(wantKey)) {
            fails++;
            System.out.println("FAIL " + label + ": expected " + wantKey + " but got " + gotKey);
        }
    }

    static List<List<Integer>> toLists(int[][] rows) {
        List<List<Integer>> lists = new ArrayList<>();
        for (int[] row : rows) {
            List<Integer> list = new ArrayList<>();
            for (int v : row) {
                list.add(v);
            }
            lists.add(list);
        }
        return lists;
    }

    static String canonical(List<List<Integer>> lists) {
        List<String> parts = new ArrayList<>();
        for (List<Integer> list : lists) {
            List<Integer> copy = new ArrayList<>(list);
            Collections.sort(copy);
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < copy.size(); i++) {
                if (i > 0) {
                    sb.append(',');
                }
                sb.append(copy.get(i));
            }
            parts.add("[" + sb + "]");
        }
        Collections.sort(parts);
        return String.join(";", parts);
    }
}