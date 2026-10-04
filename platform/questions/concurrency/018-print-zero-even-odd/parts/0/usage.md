### How it's called

```cpp
ZeroEvenOdd zeo(5);
std::vector<std::string> out;
std::mutex om;
auto record = [&](int n) { std::lock_guard<std::mutex> g(om); out.push_back(std::to_string(n)); };
auto record0 = [&](int)  { std::lock_guard<std::mutex> g(om); out.push_back("0"); };

std::thread tZ([&]{ zeo.zero(record0); });
std::thread tE([&]{ zeo.even(record); });
std::thread tO([&]{ zeo.odd (record); });
tZ.join(); tE.join(); tO.join();
// out == {"0","1","0","2","0","3","0","4","0","5"}
```
