### How it's called

```cpp
KeyedAsyncExecutor exec(4);   // 4 worker threads

std::mutex om;
std::vector<int> order_for_user_A;

for (int i = 0; i < 10; ++i) {
    exec.submit("user-A", [&, i]{
        std::lock_guard<std::mutex> g(om);
        order_for_user_A.push_back(i);   // must end up {0,1,2,...,9}, in order
    });
}
for (int i = 0; i < 100; ++i) {
    exec.submit("user-" + std::to_string(i), [i]{ handle_event(i); });  // all different keys
}

// exec's destructor runs here, blocking until every submitted task has completed
```

`submit` returns immediately (fire-and-forget); the caller never gets a handle back to
wait on an individual task.
