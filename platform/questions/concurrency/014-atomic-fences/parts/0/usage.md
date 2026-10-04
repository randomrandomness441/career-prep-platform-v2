### How it's called

Identical call shape to [[013-store-buffering]]:

```cpp
Handshake h;
int r1, r2;

std::thread ta([&]{ r1 = h.arriveA(); });
std::thread tb([&]{ r2 = h.arriveB(); });
ta.join();
tb.join();

// r1 == 1 || r2 == 1 must hold, every trial, across many thousands of fresh trials.
```
