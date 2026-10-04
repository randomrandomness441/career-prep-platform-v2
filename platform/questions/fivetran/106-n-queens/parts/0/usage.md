Your submission is a single class named `Solution`. Keep the method signature exactly as shown, because the harness compiles your file next to the tests and calls `solveNQueens` on a `Solution` instance directly.

```java
Solution solver = new Solution();
List<List<String>> boards = solver.solveNQueens(4);
System.out.println(boards.size()); // 2
```