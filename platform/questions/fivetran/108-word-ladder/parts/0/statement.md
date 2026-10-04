> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

A transformation sequence from `beginWord` to `endWord` has the form `beginWord -> s1 -> s2 -> ... -> sk`, with `k >= 1`. Every pair of adjacent words differs by exactly one letter. Every `si`, including `endWord`, must appear in `wordList`. `beginWord` does not need to be in `wordList`. Return the number of words in the shortest transformation sequence. Return 0 if no sequence exists.

```java
public int ladderLength(String beginWord, String endWord, List<String> wordList)
```

### Example 1:

```
Input: beginWord = "hit", endWord = "cog", wordList = ["hot","dot","dog","lot","log","cog"]
Output: 5
Explanation: the shortest sequence is "hit" -> "hot" -> "dot" -> "dog" -> "cog", which has 5 words.
```

### Example 2:

```
Input: beginWord = "hit", endWord = "cog", wordList = ["hot","dot","dog","lot","log"]
Output: 0
Explanation: "cog" is not in wordList, so no valid sequence exists.
```

### Constraints:

- `1 <= beginWord.length <= 10`
- `endWord.length == beginWord.length`
- `1 <= wordList.length <= 5000`
- `wordList[i].length == beginWord.length`
- `beginWord`, `endWord`, and `wordList[i]` consist of lowercase English letters.
- `beginWord != endWord`
- All the words in `wordList` are unique.
