### How it's called

```cpp
H2O h2o;
std::string out;
std::mutex om;
auto record = [&](char c) { std::lock_guard<std::mutex> g(om); out += c; };

const int n = 4;   // n oxygens, 2n hydrogens
std::vector<std::thread> ts;
for (int i = 0; i < 2 * n; ++i)
    ts.emplace_back([&]{ h2o.hydrogen([&]{ record('H'); }); });
for (int i = 0; i < n; ++i)
    ts.emplace_back([&]{ h2o.oxygen([&]{ record('O'); }); });
for (auto& t : ts) t.join();
// out, split into groups of 3, is all-H2O: every group has exactly two 'H' and one 'O'
```

All `2n + n` threads are launched and joined together; the assembler itself decides who
gets to release their atom, and in what order.
