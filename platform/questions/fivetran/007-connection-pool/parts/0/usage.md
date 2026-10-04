Your submission is a single class named `Solution`, not stateless this time -- it's the
pool itself, constructed once and shared across threads (the harness compiles it alongside
its own `Tests.java`, which also defines `MockConnection` and `ConnectionFactory`):

```java
interface MockConnection {
    boolean isValid();
    void close();
}

interface ConnectionFactory {
    MockConnection create();
}

class Solution {
    Solution(ConnectionFactory factory, int maxSize) { ... }
    MockConnection lease() throws InterruptedException { ... }
    void release(MockConnection conn) { ... }
}
```

A caller shares one pool across worker threads:

```java
Solution pool = new Solution(driverFactory, 10);

Runnable worker = () -> {
    try {
        MockConnection conn = pool.lease();
        try {
            runQuery(conn);
        } finally {
            pool.release(conn);
        }
    } catch (InterruptedException ignored) {}
};
```
