### How it's called

```cpp
result_slot slot;

std::thread producer([&slot]{
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    slot.set(42);
});
std::thread nudger([&slot]{
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(9));
        slot.nudge();          // fires repeatedly while wait_for_result is waiting
    }
});

// meanwhile, on another thread:
std::optional<int> v = slot.wait_for_result(std::chrono::milliseconds(1000));
// must return within ~1000ms of being CALLED, no matter how many nudges fired

producer.join();
nudger.join();
```

Any number of threads may call `wait_for_result` at once; all of them must see the value
once `set()` is called.
