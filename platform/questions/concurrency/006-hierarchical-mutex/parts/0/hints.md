"Per thread" is `thread_local`. One `static thread_local unsigned long` shared by every
`hierarchical_mutex` in the program, holding the level of the lowest-level mutex this thread
currently holds:

```cpp
inline static thread_local unsigned long this_thread_hierarchy_value = ULONG_MAX;
```

It needs no synchronisation of any kind — every thread has its own copy, and only that
thread ever touches it. That is the whole reason the design is cheap.
---
The previous level cannot live in the thread, because unlocks nest. A thread holding
10000 → 5000 → 1000 has to restore 5000 when it unlocks the 1000, and 10000 when it unlocks
the 5000. One variable per thread cannot hold three saved values.

So each *mutex* saves the level that was current when it was locked. The chain of saved
values is spread across the mutexes the thread holds, which is exactly the stack you need,
and it is safe to store there because only the owner can be inside `lock`/`unlock`.

Also: check before you block, not after. If you block first, a genuine inversion parks the
thread on the mutex forever and the check never runs.
---
```cpp
void lock() {
    if (this_thread_hierarchy_value <= hierarchy_value)   // <= : equal is a violation too
        throw std::logic_error("mutex hierarchy violated");
    internal_mutex.lock();
    previous_hierarchy_value = this_thread_hierarchy_value;
    this_thread_hierarchy_value = hierarchy_value;
}

void unlock() {
    this_thread_hierarchy_value = previous_hierarchy_value;
    internal_mutex.unlock();
}
```

`try_lock()` is the same check, but a failed `try_lock` must leave the thread's level alone
— only update it on the branch that actually acquired the mutex. Note that the throw
happens before `internal_mutex.lock()`, so a rejected lock has touched nothing.
