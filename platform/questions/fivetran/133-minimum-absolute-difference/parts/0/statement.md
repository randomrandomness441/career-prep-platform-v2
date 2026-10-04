> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

Given an array of **distinct** integers `arr`, find all pairs of elements with the minimum absolute difference of any two elements. Return a list of pairs in ascending order, where each pair `[a, b]` satisfies `a < b` and `b - a` equals the minimum absolute difference of any two elements in `arr`.

```java
public List<List<Integer>> minimumAbsDifference(int[] arr)
```

### Example 1:

```
Input: arr = [4,2,1,3]
Output: [[1,2],[2,3],[3,4]]
```

### Example 2:

```
Input: arr = [1,3,6,10,15]
Output: [[1,3]]
```

### Example 3:

```
Input: arr = [3,8,-10,23,19,-4,-14,27]
Output: [[-14,-10],[19,23],[23,27]]
```

### Constraints:

- `2 <= arr.length <= 10^5`
- `-10^6 <= arr[i] <= 10^6`
- All elements in `arr` are distinct
