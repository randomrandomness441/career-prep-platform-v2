> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

The string `s` is written in a zigzag pattern on `numRows` rows. You write characters straight down the first column, then move diagonally up and to the right until you reach the top row, then go straight down again. Reading every row from left to right gives the converted string, and you return it.

For example, `"PAYPALISHIRING"` with 3 rows looks like this:

```
P   A   H   N
A P L S I I G
Y   I   R
```

```java
public String convert(String s, int numRows)
```

### Example 1:

```
Input: s = "PAYPALISHIRING", numRows = 3
Output: "PAHNAPLSIIGYIR"
```

### Example 2:

```
Input: s = "PAYPALISHIRING", numRows = 4
Output: "PINALSIGYAHRPI"
```

### Example 3:

```
Input: s = "A", numRows = 1
Output: "A"
```

### Constraints:

- `1 <= s.length <= 1000`
- `s` consists of English letters (lower-case and upper-case), ',' and '.'
- `1 <= numRows <= 1000`
