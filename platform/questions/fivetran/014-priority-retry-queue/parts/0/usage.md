Your submission is a single class named `Solution`, stateful (the harness compiles it
alongside its own `Tests.java`, which also defines `Job`):

```java
class Job {
    final String id;
    final int priority;
    Job(String id, int priority) { this.id = id; this.priority = priority; }
}

class Solution {
    Solution(int maxAttempts, long baseBackoffMillis) { ... }
    void submit(Job job) { ... }
    Job poll(long nowMillis) { ... }
    void fail(Job job, long nowMillis) { ... }
    void complete(Job job) { ... }
}
```

A worker loop looks like this (real code would read the clock; tests pass `nowMillis` in
directly):

```java
Solution queue = new Solution(5, 1000);
queue.submit(new Job("sync-42", /* priority */ 10));

Job job = queue.poll(System.currentTimeMillis());
if (job != null) {
    try {
        runSync(job);
        queue.complete(job);
    } catch (Exception e) {
        queue.fail(job, System.currentTimeMillis());
    }
}
```
