import java.util.*;

class Solution {
    public boolean isCompleteTree(TreeNode root) {
        Queue<TreeNode> q = new LinkedList<>();
        q.add(root);
        boolean sawNull = false;
        while (!q.isEmpty()) {
            TreeNode cur = q.poll();
            if (cur == null) {
                sawNull = true;
            } else {
                if (sawNull) return false;
                q.add(cur.left);
                q.add(cur.right);
            }
        }
        return true;
    }
}