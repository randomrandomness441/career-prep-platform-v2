### How it's called

```cpp
SlotPool pool(3);   // 3 slots, 50 threads competing for them

std::vector<std::thread> ts;
for (int i = 0; i < 50; ++i) {
    ts.emplace_back([&pool]{
        pool.acquire();
        use_connection();
        pool.release();
    });
}
for (auto& t : ts) t.join();

std::printf("started=%ld wasted_wakeups=%ld\n",
            pool.started.load(), pool.wasted_wakeups.load());
```

At no point do more than 3 threads hold a slot at once; `wasted_wakeups` is graded on how
small it stays relative to a design that just calls `notify_all()` on every `release()`.
