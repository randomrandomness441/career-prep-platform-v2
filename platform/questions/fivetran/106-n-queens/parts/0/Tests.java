import java.util.*;

public class Tests {

    public static void main(String[] args) {
        int fails = 0;

        // Smallest allowed board: exactly one way.
        fails += checkExact(1, Arrays.asList(
                Arrays.asList("Q")
        ));

        // Edge cases: no valid placement exists for n = 2 or n = 3.
        fails += checkCount(2, 0);
        fails += checkCount(3, 0);

        // Classic n = 4 result, compared as an exact set of boards.
        fails += checkExact(4, Arrays.asList(
                Arrays.asList(".Q..", "...Q", "Q...", "..Q."),
                Arrays.asList("..Q.", "Q...", "...Q", ".Q..")
        ));

        // Known solution counts for the remaining n.
        fails += checkCount(5, 10);
        fails += checkCount(6, 4);
        fails += checkCount(7, 40);
        fails += checkCount(8, 92);
        fails += checkCount(9, 352);

        // Every board returned for n = 8 must be a legal placement.
        int invalidBoards = 0;
        for (List<String> board : new Solution().solveNQueens(8)) {
            if (!isValidBoard(board)) {
                invalidBoards++;
            }
        }
        if (invalidBoards > 0) {
            System.out.println("FAILED n=8 validity: " + invalidBoards + " boards have attacking queens");
            fails++;
        } else {
            System.out.println("ok n=8 validity");
        }

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    private static int checkExact(int n, List<List<String>> expected) {
        List<List<String>> actual = canon(new Solution().solveNQueens(n));
        List<List<String>> want = canon(expected);
        if (!actual.equals(want)) {
            System.out.println("FAILED n=" + n + " exact boards");
            System.out.println("  expected: " + want);
            System.out.println("  actual:   " + actual);
            return 1;
        }
        System.out.println("ok n=" + n + " exact boards");
        return 0;
    }

    private static int checkCount(int n, int expectedCount) {
        List<List<String>> actual = new Solution().solveNQueens(n);
        if (actual.size() != expectedCount) {
            System.out.println("FAILED n=" + n + " count: expected " + expectedCount + ", got " + actual.size());
            return 1;
        }
        System.out.println("ok n=" + n + " count: " + expectedCount);
        return 0;
    }

    // Sort so the order of boards inside the result does not matter.
    private static List<List<String>> canon(List<List<String>> boards) {
        List<List<String>> copy = new ArrayList<>();
        for (List<String> board : boards) {
            copy.add(new ArrayList<>(board));
        }
        copy.sort((a, b) -> String.join("", a).compareTo(String.join("", b)));
        return copy;
    }

    private static boolean isValidBoard(List<String> board) {
        int n = board.size();
        for (int row = 0; row < n; row++) {
            for (int col = 0; col < n; col++) {
                if (board.get(row).charAt(col) != 'Q') {
                    continue;
                }
                for (int below = row + 1; below < n; below++) {
                    if (board.get(below).charAt(col) == 'Q') return false;
                    int shift = below - row;
                    if (col - shift >= 0 && board.get(below).charAt(col - shift) == 'Q') return false;
                    if (col + shift < n && board.get(below).charAt(col + shift) == 'Q') return false;
                }
            }
        }
        return true;
    }
}