### How it's called

```cpp
EventGate gate;
std::vector<int> order;
std::mutex om;
auto record = [&](int id) { std::lock_guard<std::mutex> g(om); order.push_back(id); };

gate.reg_cb([&]{ record(0); });   // no event in progress -- runs immediately, order == {0}

gate.begin_event();               // called from whatever thread manages the event
gate.reg_cb([&]{ record(1); });   // event in progress -- queued, does NOT run yet
gate.reg_cb([&]{ record(2); });   // also queued
// order is still {0} here -- 1 and 2 have not run

gate.end_event();                 // drains the waiting list: runs 1, then 2, in order
// order == {0, 1, 2}

gate.reg_cb([&]{ record(3); });   // event already over -- runs immediately
// order == {0, 1, 2, 3}
```

In the concurrent case, many threads call `reg_cb` while a separate thread calls
`begin_event()`/`end_event()` on its own schedule; every registered callback still runs
exactly once.
