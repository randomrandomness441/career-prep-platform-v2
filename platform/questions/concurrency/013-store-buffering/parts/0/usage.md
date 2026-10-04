### How it's called

```cpp
Handshake h;
int r1, r2;

std::thread ta([&]{ r1 = h.arriveA(); });
std::thread tb([&]{ r2 = h.arriveB(); });
ta.join();
tb.join();

// r1 == 1 || r2 == 1 must hold -- always, across many thousands of repeated trials
// with a FRESH Handshake and fresh threads each time (the bug is a race, so it
// needs a clean re-run to have a chance to reappear).
```

Each `Handshake` is used exactly once: one call to `arriveA` on one thread, one call to
`arriveB` on another, both racing to finish.
