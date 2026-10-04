### How it's called

```cpp
hierarchical_mutex high(10000), mid(5000), low(1000);

high.lock();
mid.lock();          // ok: 5000 < 10000
low.lock();          // ok: 1000 < 5000
low.unlock();
mid.unlock();
high.unlock();

// works with std::lock_guard / std::unique_lock like any other mutex:
{
    std::lock_guard<hierarchical_mutex> lk(high);
}

// the violation:
mid.lock();
try {
    high.lock();      // going back UP (10000 after 5000) -- throws std::logic_error
} catch (const std::logic_error&) {
    // mid is still held; high was never acquired
}
mid.unlock();
```
