Your submission is a single class named Solution. It holds searchMatrix and nothing else. The grader instantiates it and calls the method directly, so keep the signature exactly as given.

A worked call site:

```java
Solution s = new Solution();
boolean hit = s.searchMatrix(new int[][]{{1,3,5,7},{10,11,16,20},{23,30,34,60}}, 3);
// hit == true
```