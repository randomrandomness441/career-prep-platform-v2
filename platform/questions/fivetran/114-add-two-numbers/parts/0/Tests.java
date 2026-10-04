import java.util.*;

public class Tests {
    static int fails = 0;

    public static void main(String[] args) {
        check(new int[]{2, 4, 3}, new int[]{5, 6, 4}, new int[]{7, 0, 8});
        check(new int[]{0}, new int[]{0}, new int[]{0});
        check(new int[]{5}, new int[]{5}, new int[]{0, 1});
        check(new int[]{1, 8}, new int[]{0}, new int[]{1, 8});
        check(new int[]{9, 9, 9, 9, 9, 9, 9}, new int[]{9, 9, 9, 9}, new int[]{8, 9, 9, 9, 0, 0, 0, 1});
        check(new int[]{9}, new int[]{1, 9, 9, 9, 9, 9, 9, 9, 9, 9}, new int[]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});

        // Max-length edge case: 100 nines plus 100 nines.
        int[] nines = new int[100];
        Arrays.fill(nines, 9);
        int[] want = new int[101];
        want[0] = 8;
        for (int i = 1; i <= 99; i++) {
            want[i] = 9;
        }
        want[100] = 1;
        check(nines, nines, want);

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        } else {
            System.out.println("all checks passed");
        }
    }

    static void check(int[] a, int[] b, int[] expected) {
        ListNode got = new Solution().addTwoNumbers(build(a), build(b));
        ListNode want = build(expected);
        if (!eq(got, want)) {
            fails++;
            System.out.println("FAIL: addTwoNumbers(" + Arrays.toString(a) + ", " + Arrays.toString(b)
                    + ") expected " + str(want) + " but got " + str(got));
        }
    }

    static ListNode build(int[] vals) {
        ListNode dummy = new ListNode(0);
        ListNode cur = dummy;
        for (int v : vals) {
            cur.next = new ListNode(v);
            cur = cur.next;
        }
        return dummy.next;
    }

    static boolean eq(ListNode a, ListNode b) {
        while (a != null && b != null) {
            if (a.val != b.val) {
                return false;
            }
            a = a.next;
            b = b.next;
        }
        return a == null && b == null;
    }

    static String str(ListNode head) {
        StringBuilder sb = new StringBuilder("[");
        while (head != null) {
            sb.append(head.val);
            if (head.next != null) {
                sb.append(",");
            }
            head = head.next;
        }
        return sb.append("]").toString();
    }
}

class ListNode {
    int val;
    ListNode next;

    ListNode() {
    }

    ListNode(int val) {
        this.val = val;
    }

    ListNode(int val, ListNode next) {
        this.val = val;
        this.next = next;
    }
}