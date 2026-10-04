import java.util.*;

public class Tests {
    static int fails = 0;

    static TreeNode build(Integer[] vals) {
        if (vals == null || vals.length == 0) return null;
        TreeNode root = new TreeNode(vals[0]);
        Queue<TreeNode> q = new LinkedList<>();
        q.add(root);
        int i = 1;
        while (i < vals.length && !q.isEmpty()) {
            TreeNode cur = q.poll();
            if (vals[i] != null) {
                cur.left = new TreeNode(vals[i]);
                q.add(cur.left);
            }
            i++;
            if (i < vals.length) {
                if (vals[i] != null) {
                    cur.right = new TreeNode(vals[i]);
                    q.add(cur.right);
                }
                i++;
            }
        }
        return root;
    }

    static void check(String name, boolean expected, boolean actual) {
        if (expected != actual) {
            System.out.println("FAIL: " + name + ": expected " + expected + " but got " + actual);
            fails++;
        }
    }

    public static void main(String[] args) {
        Solution s = new Solution();

        check("single node", true, s.isCompleteTree(build(new Integer[]{1})));
        check("two nodes, left child only", true, s.isCompleteTree(build(new Integer[]{1, 2})));
        check("right child only", false, s.isCompleteTree(build(new Integer[]{1, null, 2})));
        check("example 1, all levels full", true, s.isCompleteTree(build(new Integer[]{1, 2, 3, 4, 5, 6})));
        check("example 2, gap before 7", false, s.isCompleteTree(build(new Integer[]{1, 2, 3, 4, 5, null, 7})));
        check("last level shifted right", false, s.isCompleteTree(build(new Integer[]{1, 2, 3, null, null, 7, 8})));
        check("missing right child but node below left subtree", false, s.isCompleteTree(build(new Integer[]{1, 2, 3, 4, 5, 6, null, 7})));
        check("perfect tree of 7", true, s.isCompleteTree(build(new Integer[]{1, 2, 3, 4, 5, 6, 7})));
        check("partial last level filled from left", true, s.isCompleteTree(build(new Integer[]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12})));

        Integer[] perfect = new Integer[15];
        for (int i = 0; i < 15; i++) perfect[i] = i + 1;
        check("perfect tree of 15", true, s.isCompleteTree(build(perfect)));

        if (fails > 0) {
            System.out.println(fails + " test(s) failed");
            System.exit(1);
        }
        System.out.println("all checks passed");
    }
}

class TreeNode {
    int val;
    TreeNode left;
    TreeNode right;
    TreeNode() {}
    TreeNode(int val) { this.val = val; }
    TreeNode(int val, TreeNode left, TreeNode right) {
        this.val = val;
        this.left = left;
        this.right = right;
    }
}