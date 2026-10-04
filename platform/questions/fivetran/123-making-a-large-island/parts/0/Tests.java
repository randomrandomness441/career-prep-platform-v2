public class Tests {

    public static void main(String[] args) {
        int fails = 0;

        fails += check("1x1 grid holding a single 0", new int[][]{{0}}, 1);
        fails += check("1x1 grid holding a single 1", new int[][]{{1}}, 1);
        fails += check("diagonal islands join through one flip", new int[][]{{1, 0}, {0, 1}}, 3);
        fails += check("L shape with one 0", new int[][]{{1, 1}, {1, 0}}, 4);
        fails += check("all 1s, no flip available", new int[][]{{1, 1}, {1, 1}}, 4);
        fails += check("3x3 of all 0s", new int[][]{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, 1);
        fails += check("four islands meet at the center", new int[][]{{0, 1, 0}, {1, 0, 1}, {0, 1, 0}}, 5);
        fails += check("one island touches the center 0 from every side", new int[][]{{1, 1, 1}, {1, 0, 1}, {1, 1, 1}}, 9);

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static int check(String name, int[][] grid, int expected) {
        int actual = new Solution().largestIsland(grid);
        if (actual != expected) {
            System.out.println("FAIL: " + name + " expected " + expected + ", got " + actual);
            return 1;
        }
        return 0;
    }
}