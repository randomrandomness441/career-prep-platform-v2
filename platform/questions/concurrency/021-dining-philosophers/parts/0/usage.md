### How it's called

```cpp
DiningPhilosophers table;

std::vector<std::thread> philosophers;
for (int i = 0; i < 5; ++i) {
    philosophers.emplace_back([&table, i]{
        for (int meal = 0; meal < 100; ++meal) {
            table.wantsToEat(i,
                [&]{ pick_fork(i);           },   // pickLeftFork
                [&]{ pick_fork((i + 4) % 5); },   // pickRightFork
                [&]{ eat(i);                 },   // eat
                [&]{ put_fork(i);            },   // putLeftFork
                [&]{ put_fork((i + 4) % 5);  });  // putRightFork
        }
    });
}
for (auto& t : philosophers) t.join();
```

Five threads, one per philosopher, each calling `wantsToEat` repeatedly and concurrently
with the other four.
