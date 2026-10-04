> Standard LeetCode problem, tagged to Fivetran's interview loop. Solve it in Java, no different from solving it on leetcode.com.

A transaction is possibly invalid if its amount exceeds $1000, or if it happens no more than 60 minutes away from another transaction with the same name in a different city. You are given an array of strings `transactions` where `transactions[i]` takes the form `"{name},{time},{amount},{city}"`. Return a list of all possibly invalid transactions. The answer may be in any order.

```java
public List<String> invalidTransactions(String[] transactions)
```

### Example 1:

```
Input: transactions = ["alice,20,800,mtv","alice,50,100,beijing"]
Output: ["alice,20,800,mtv","alice,50,100,beijing"]
Explanation: The two entries share a name and sit 30 minutes apart in different cities, so both are invalid.
```

### Example 2:

```
Input: transactions = ["alice,20,800,mtv","alice,50,1200,mtv"]
Output: ["alice,50,1200,mtv"]
Explanation: The amount of 1200 breaks the limit. The first entry is only 30 minutes from the same name, but both are in mtv, so it stays valid.
```

### Example 3:

```
Input: transactions = ["alice,20,800,mtv","bob,50,1200,mtv"]
Output: ["bob,50,1200,mtv"]
Explanation: Different names never trigger the time rule, and only bob exceeds 1000.
```

### Constraints:

- `1 <= transactions.length <= 1000`
- Each `transactions[i]` takes the form `"{name},{time},{amount},{city}"`.
- Each name and city consists of lowercase English letters and has length between 1 and 10.
- `0 <= time <= 1000`
- `0 <= amount <= 2000`
