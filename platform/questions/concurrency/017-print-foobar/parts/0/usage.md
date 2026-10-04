### How it's called

```cpp
FooBar fb(3);
std::string out;
std::mutex om;
auto record = [&](const char* s) { std::lock_guard<std::mutex> g(om); out += s; };

std::thread tA([&]{ fb.foo([&]{ record("foo"); }); });
std::thread tB([&]{ fb.bar([&]{ record("bar"); }); });
tA.join();
tB.join();
// out == "foobarfoobarfoobar"
```

Each thread calls its method exactly once; the method itself loops internally `n` times.
