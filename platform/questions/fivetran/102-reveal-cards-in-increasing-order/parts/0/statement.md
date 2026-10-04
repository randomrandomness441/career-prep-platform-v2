> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

You are given an integer array `deck` where `deck[i]` is the value on the i-th card. Every value in `deck` is unique and all cards start face down. Repeat the following until all cards are revealed: reveal the top card, then move the next top card to the bottom of the deck. Return an ordering of the deck that reveals the cards in strictly increasing order.

```java
public int[] deckRevealedIncreasing(int[] deck)
```

### Example 1:

```
Input: deck = [17,13,11,2,3,5,7]
Output: [2,13,3,11,5,17,7]
```

### Example 2:

```
Input: deck = [1,1000]
Output: [1,1000]
```

### Example 3:

```
Input: deck = [3,1,2]
Output: [1,3,2]
```

### Constraints:

- `1 <= deck.length <= 1000`
- `1 <= deck[i] <= 10^6`
- All values in `deck` are unique.
