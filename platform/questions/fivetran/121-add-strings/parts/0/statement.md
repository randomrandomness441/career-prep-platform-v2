> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

Given two non-negative integers `num1` and `num2` represented as strings, return the sum of `num1` and `num2` as a string. You must solve it without using any built-in library for handling large integers, such as `BigInteger`. You must also not convert the inputs to integers directly.

```java
public String addStrings(String num1, String num2)
```

### Example 1:

```
Input: num1 = "11", num2 = "123"
Output: "134"
```

### Example 2:

```
Input: num1 = "456", num2 = "77"
Output: "533"
```

### Example 3:

```
Input: num1 = "0", num2 = "0"
Output: "0"
```

### Constraints:

- `1 <= num1.length, num2.length <= 10^4`
- `num1` and `num2` consist of only digits.
- `num1` and `num2` do not contain any leading zeros except for the zero itself.
