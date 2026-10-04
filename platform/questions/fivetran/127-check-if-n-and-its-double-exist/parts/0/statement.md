Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

Given an array `arr` of integers, check if there exist two indices `i` and `j` such that:

- `i != j`
- `0 <= i, j < arr.length`
- `arr[i] == 2 * arr[j]`

Return `true` if such indices exist, otherwise return `false`.

```java
public boolean checkIfExist(int[] arr)
```

Input: arr = [10,2,5,3]
Output: true
Explanation: N = 10 is the double of M = 5, that is, 10 = 2 * 5

Input: arr = [7,1,14,11]
Output: true
Explanation: N = 14 is the double of M = 7, that is, 14 = 2 * 7

Input: arr = [3,1,7,11]
Output: false

Constraints:
- 2 <= arr.length <= 500
- -10^3 <= arr[i] <= 10^3