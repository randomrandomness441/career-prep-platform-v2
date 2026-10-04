Your submission is a single non-public class named `Solution` with the method `continuousSubarrays`. The test harness constructs one `Solution` instance and calls the method on plain `int` arrays. A worked call site:

```java
Solution sol = new Solution();
int[] nums = {5, 4, 2, 4};
long count = sol.continuousSubarrays(nums); // 8
```

The answer can exceed the int range, so the return type is `long`.