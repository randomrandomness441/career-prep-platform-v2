### How it's called

```cpp
RobotCleaner grid(10, 10);

std::vector<std::thread> robots;
robots.emplace_back([&]{ for (int i = 0; i < 500; ++i) grid.move(2, 3, 2, 4); });
robots.emplace_back([&]{ for (int i = 0; i < 500; ++i) grid.move(2, 4, 2, 3); });  // swap case
robots.emplace_back([&]{ for (int i = 0; i < 500; ++i) grid.move(0, 0, 0, 1); });
for (auto& t : robots) t.join();   // must complete -- no deadlock, no matter the interleaving
```

Many robot threads call `move` concurrently on overlapping and adjacent cells, including
the two-robots-swapping-cells pattern above.
