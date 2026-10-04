### How it's called

```cpp
std::vector<std::thread> ts;
for (int i = 0; i < 8; ++i) {
    ts.emplace_back([]{
        int id = ticket_station::station_id();     // assigned once, stable for this thread
        for (int k = 0; k < 100; ++k) {
            long t = ticket_station::next_ticket(); // 1, 2, 3, ... for THIS thread only
            record(id, t);
        }
        // ticket_station::issued_here() == 100 here
    });
}
for (auto& t : ts) t.join();
```

No object is ever passed to the threads — every call is a bare static function, and each
thread's answers are independent of what the other seven are doing.
