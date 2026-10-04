Your submission is a single class named Solution. You may import java.util and nothing else. A typical call site:

```java
Solution s = new Solution();
TreeNode root = new TreeNode(1);
root.left = new TreeNode(2);
root.right = new TreeNode(3);
boolean ok = s.isCompleteTree(root); // true
```

Tree inputs in the examples are level-order arrays, so [1,2,3,4,5,null,7] means node 3 has no left child and a right child of 7.