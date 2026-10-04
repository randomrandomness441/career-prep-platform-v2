Your submission is a single class named `Solution`. No package declaration and no imports beyond `java.util.*` are needed. Do not print anything inside `ladderLength`; just return the value.

Worked call site:

```java
List<String> wordList = Arrays.asList("hot", "dot", "dog", "lot", "log", "cog");
Solution solution = new Solution();
int length = solution.ladderLength("hit", "cog", wordList);
// length == 5, because "hit" -> "hot" -> "dot" -> "dog" -> "cog" has 5 words
```