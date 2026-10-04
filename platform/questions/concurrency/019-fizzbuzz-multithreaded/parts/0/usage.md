### How it's called

```cpp
FizzBuzz fb(15);
std::vector<std::string> out;
std::mutex om;
auto push = [&](std::string s) { std::lock_guard<std::mutex> g(om); out.push_back(std::move(s)); };

std::thread t1([&]{ fb.fizz([&]{ push("fizz"); }); });
std::thread t2([&]{ fb.buzz([&]{ push("buzz"); }); });
std::thread t3([&]{ fb.fizzbuzz([&]{ push("fizzbuzz"); }); });
std::thread t4([&]{ fb.number([&](int n){ push(std::to_string(n)); }); });
t1.join(); t2.join(); t3.join(); t4.join();
// out == {"1","2","fizz","4","buzz","fizz","7","8","fizz","buzz","11","fizz","13","14","fizzbuzz"}
```
