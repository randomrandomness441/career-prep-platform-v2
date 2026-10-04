### How it's called

```cpp
{
    WorkStealingPool pool(4);

    for (int i = 0; i < 1000; ++i)
        pool.submit([i]{ do_work(i); });    // spread across the 4 workers' own queues

    pool.submit([]{ last_minute_task(); });  // still runs even submitted late
}   // destructor blocks here until every submitted task has actually run
```

`submit` may be called from any thread, including from inside a running task; the pool
itself owns and manages its worker threads internally.
