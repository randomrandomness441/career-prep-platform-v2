### How it's called

Same call shape as [[021-dining-philosophers]], but philosopher 0's thread loops with no
delay between meals while philosophers 1-4 sleep a bit between meals:

```cpp
DiningPhilosophers table;
std::atomic<long> meals[5] = {};

std::vector<std::thread> philosophers;
for (int i = 0; i < 5; ++i) {
    philosophers.emplace_back([&, i]{
        for (int round = 0; round < 5000; ++round) {
            table.wantsToEat(i, pick_left(i), pick_right(i), [&]{ meals[i]++; },
                              put_left(i), put_right(i));
            if (i != 0) std::this_thread::sleep_for(std::chrono::microseconds(200));
            // philosopher 0: no sleep -- immediately wants to eat again
        }
    });
}
for (auto& t : philosophers) t.join();
// meals[0] must stay within a bounded multiple of the average of meals[1..4]
```
