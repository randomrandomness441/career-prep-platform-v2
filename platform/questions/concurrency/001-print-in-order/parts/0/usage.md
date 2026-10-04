### How it's called

The harness builds one `Foo`, shared by three threads, and starts them in a
shuffled order — your code is what puts the output back in order, not the
thread start order.

```cpp
Foo foo;

std::thread tA([&foo]{ foo.first ([]{ std::cout << "first";  }); });
std::thread tB([&foo]{ foo.second([]{ std::cout << "second"; }); });
std::thread tC([&foo]{ foo.third ([]{ std::cout << "third";  }); });

// started in a random permutation of A/B/C across runs
tA.join();
tB.join();
tC.join();
// stdout must always read: firstsecondthird
```

Each thread calls exactly one method, exactly once. The callback passed in
(`printFirst`/`printSecond`/`printThird`) is what actually writes the output —
your job is only to control *when* each thread is allowed to call it.
