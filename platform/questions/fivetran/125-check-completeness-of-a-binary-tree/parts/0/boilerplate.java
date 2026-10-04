import java.util.*;

class Solution {
    public boolean isCompleteTree(TreeNode root) {
        Queue<TreeNode> q = new LinkedList<>();
        q.add(root);
        while (!q.isEmpty()) {
            int size = q.size();
            boolean sawGap = false;
            for (int i = 0; i < size; i++) {
                TreeNode cur = q.poll();
                if (cur == null) {
                    sawGap = true;
                } else {
                    if (sawGap) return false;
                    q.add(cur.left);
                    q.add(cur.right);
                }
            }
        }
        return true;
    }
}