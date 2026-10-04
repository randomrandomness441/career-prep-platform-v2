class Solution {
    public static Node connect(Node root) {
        if (root == null) {
            return root;
        }
        // Walk level by level using the next pointers already built on the
        // level above, so no queue and no extra space.
        Node leftmost = root;
        while (leftmost.left != null) {
            Node head = leftmost;
            while (head != null) {
                head.left.next = head.right;
                if (head.next != null) {
                    head.right.next = head.next.left;
                }
                head = head.next;
            }
            leftmost = leftmost.left;
        }
        return root;
    }
}