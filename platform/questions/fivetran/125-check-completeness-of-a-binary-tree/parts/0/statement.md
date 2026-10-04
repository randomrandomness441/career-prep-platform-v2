Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

Given the root of a binary tree, determine if it is a complete binary tree. In a complete binary tree, every level is completely filled except possibly the last. When the last level is not full, all of its nodes are as far left as possible.

public boolean isCompleteTree(TreeNode root)

Input: root = [1,2,3,4,5,6] / Output: true
Input: root = [1,2,3,4,5,null,7] / Output: false
Input: root = [1,null,2] / Output: false

Constraints:
- The number of nodes in the tree is in the range [1, 100].
- 1 <= Node.val <= 100