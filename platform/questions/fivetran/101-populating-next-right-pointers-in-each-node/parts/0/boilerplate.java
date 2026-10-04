class Solution {
    public static Node connect(Node root) {
        if (root == null || root.left == null) {
            return root;
        }
        root.left.next = root.right;
        connect(root.left);
        connect(root.right);
        return root;
    }
}