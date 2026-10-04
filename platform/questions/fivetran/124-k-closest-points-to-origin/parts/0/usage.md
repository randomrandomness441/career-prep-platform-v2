Your submission is a single class named `Solution`. The grader instantiates it and calls the method directly, so do not read stdin and do not print anything. Put any helpers you want inside that one class.

A worked call site:

```java
Solution s = new Solution();
int[][] pts = {{1, 3}, {-2, 2}, {5, 4}};
int[][] ans = s.kClosest(pts, 1);
// ans holds [[-2,2]] because its squared distance of 8 is the smallest
```