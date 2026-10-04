Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

You are given a perfect binary tree, so every leaf sits on the same level and every parent has exactly two children. Each node carries an extra next pointer that starts out null. Set each next pointer to the node immediately to its right on the same level. If there is no right neighbor, leave next as null. Return the root when you are done.

public static Node connect(Node root)

Input: root = [1,2,3,4,5,6,7] / Output: [1,#,2,3,#,4,5,6,7,#]
Input: root = [] / Output: []

Constraints:
- The number of nodes in the tree is in the range [0, 2^12 - 1].
- -1000 <= Node.val <= 1000
- The tree is always perfect, so you never need to handle a ragged level.
- Follow-up: try to use O(1) extra space. The recursive call stack does not count.