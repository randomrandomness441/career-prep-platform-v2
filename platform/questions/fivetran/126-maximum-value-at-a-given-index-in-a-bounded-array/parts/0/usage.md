Your submission is a single class named `Solution` containing the method `maxValue`. The grader compiles your file in the same package as its test harness and calls the method on an instance, so keep everything in one class with no extra public types.

A worked call site:

```java
Solution sol = new Solution();
int peak = sol.maxValue(4, 2, 6); // returns 2, achieved by nums = [1,2,2,1]
System.out.println(peak);
```

Return the value of `nums[index]` as an int. Do not return the array itself.