import java.util.*;

public class Tests {

    static int fails = 0;

    public static void main(String[] args) {
        check(new int[]{17, 13, 11, 2, 3, 5, 7}, new int[]{2, 13, 3, 11, 5, 17, 7});
        check(new int[]{1, 1000}, new int[]{1, 1000});
        check(new int[]{1}, new int[]{1});
        check(new int[]{3, 1, 2}, new int[]{1, 3, 2});
        check(new int[]{5, 4, 3, 2, 1}, new int[]{1, 5, 2, 4, 3});
        check(new int[]{8, 1, 4, 6, 2, 7, 3, 5}, new int[]{1, 5, 2, 7, 3, 6, 4, 8});
        check(new int[]{1000000, 999999, 1, 500000}, new int[]{1, 999999, 500000, 1000000});

        // Max-size case from the constraints: 1000 unique cards.
        int n = 1000;
        int[] big = new int[n];
        for (int i = 0; i < n; i++) {
            big[i] = (i * 7) % n + 1;
        }
        int[] got = new Solution().deckRevealedIncreasing(big);
        if (!revealsIncreasing(got, big)) {
            fails++;
            System.out.println("FAILED: n=1000 deck does not reveal in increasing order");
        }

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }

    static void check(int[] deck, int[] expected) {
        int[] got = new Solution().deckRevealedIncreasing(deck);
        if (!Arrays.equals(got, expected)) {
            fails++;
            System.out.println("FAILED: deck=" + Arrays.toString(deck));
            System.out.println("  expected " + Arrays.toString(expected));
            System.out.println("  got      " + Arrays.toString(got));
        }
    }

    // Simulates the reveal process on the candidate ordering and confirms it is a
    // permutation of the original deck that comes out in increasing order.
    static boolean revealsIncreasing(int[] ordered, int[] original) {
        if (ordered.length != original.length) {
            return false;
        }
        int[] a = ordered.clone();
        int[] b = original.clone();
        Arrays.sort(a);
        Arrays.sort(b);
        if (!Arrays.equals(a, b)) {
            return false;
        }
        Deque<Integer> dq = new ArrayDeque<>();
        for (int v : ordered) {
            dq.addLast(v);
        }
        int prev = Integer.MIN_VALUE;
        while (!dq.isEmpty()) {
            int top = dq.pollFirst();
            if (top <= prev) {
                return false;
            }
            prev = top;
            if (!dq.isEmpty()) {
                dq.addLast(dq.pollFirst());
            }
        }
        return true;
    }
}