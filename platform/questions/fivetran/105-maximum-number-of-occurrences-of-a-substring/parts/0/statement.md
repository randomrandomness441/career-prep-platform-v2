Standard LeetCode problem, tagged to Fivetran's interview loop -- solve it in Java, no different from solving it on leetcode.com.

Given a string `s`, return the maximum number of occurrences of any substring under the following rules. The number of unique characters in the substring must be less than or equal to `maxLetters`. The substring size must be between `minSize` and `maxSize` inclusive. Occurrences may overlap.

`public int maxFreq(String s, int maxLetters, int minSize, int maxSize)`

Example 1:
Input: s = "aababcaab", maxLetters = 2, minSize = 3, maxSize = 4
Output: 2
Explanation: "aab" has 2 unique letters and appears twice, at index 0 and index 6.

Example 2:
Input: s = "aaaa", maxLetters = 1, minSize = 3, maxSize = 3
Output: 2
Explanation: "aaa" appears twice.

Example 3:
Input: s = "abcde", maxLetters = 2, minSize = 3, maxSize = 3
Output: 0
Explanation: Every size-3 substring has 3 unique letters, so none qualifies.

Constraints:
- s.length() is in the range [1, 10^5].
- 1 <= maxLetters <= 26
- 1 <= minSize <= maxSize <= min(26, s.length())
- s consists of only lowercase English letters.