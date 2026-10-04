Your submission is a single class named `Solution`, stateful, constructed once and reused
(the harness compiles it alongside its own `Tests.java`):

```java
class Solution {
    Solution(List<String> workers, int virtualNodesPerWorker) { ... }
    String assign(String key) { ... }
    void addWorker(String worker) { ... }
    void removeWorker(String worker) { ... }
}
```

A caller keeps one instance alive across the fleet's lifetime and mutates it as workers
come and go:

```java
Solution ring = new Solution(List.of("w1", "w2", "w3"), 150);
String owner = ring.assign("connector-42");

ring.removeWorker("w2"); // only connectors that were on w2 move
String newOwner = ring.assign("connector-42");
```
