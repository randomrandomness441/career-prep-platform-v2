Your submission is a single non-public class named `Solution`. Keep `invalidTransactions` static so the grader can call it as `Solution.invalidTransactions`. One worked call site:

```java
String[] transactions = {"alice,20,800,mtv", "alice,50,1200,mtv"};
List<String> invalid = Solution.invalidTransactions(transactions);
System.out.println(invalid); // [alice,50,1200,mtv]
```