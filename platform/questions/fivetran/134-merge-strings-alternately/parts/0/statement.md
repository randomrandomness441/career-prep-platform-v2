> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

You are given two strings `word1` and `word2`. Merge them by adding letters in alternating order, starting with `word1`. If one string is longer than the other, append the additional letters onto the end of the merged string.

```java
public String mergeAlternately(String word1, String word2)
```

### Example 1:

```
Input: word1 = "abc", word2 = "pqr"
Output: "apbqcr"
Explanation: The merged string is a + p + b + q + c + r.
```

### Example 2:

```
Input: word1 = "ab", word2 = "pqrs"
Output: "apbqrs"
```

### Example 3:

```
Input: word1 = "abcd", word2 = "pq"
Output: "apbqcd"
```

### Constraints:

- `1 <= word1.length, word2.length <= 100`
- `word1` and `word2` consist of lowercase English letters.
