### How it's called

```cpp
threadsafe_lookup_table<int, int> table(19);   // 19 buckets

std::vector<std::thread> readers, writers;
for (int i = 0; i < 20; ++i)
    readers.emplace_back([&]{ for (int n = 0; n < 1000; ++n) (void)table.value_for(n % 50, -1); });
for (int i = 0; i < 4; ++i)
    writers.emplace_back([&, i]{ table.add_or_update_mapping(i, i * 10); });

for (auto& t : readers) t.join();
for (auto& t : writers) t.join();
// table.value_for(i) == i * 10 for each i that was written; -1 for anything never written

table.remove_mapping(2);
// table.value_for(2, -1) == -1 afterward; removing a missing key is harmless
```
