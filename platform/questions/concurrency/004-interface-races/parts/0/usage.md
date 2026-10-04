### How it's called

```cpp
threadsafe_stack s;

std::thread pusher([&s]{ for (int i = 0; i < 100; ++i) s.push(i); });
std::thread popper([&s]{
    int drained = 0;
    while (drained < 100) {
        if (std::optional<int> v = s.pop()) ++drained;   // one call, no separate empty() check
    }
});
pusher.join();
popper.join();
```

Many threads call `push` and `pop` concurrently; no two `pop()` calls may ever return the
same element.
