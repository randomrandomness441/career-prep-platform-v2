### How it's called

Same `Foo` as part 0, three threads, one shuffled start order — except this time one of
the callbacks throws, and the harness checks that the exception comes back out to the
thread that called it, while the other two threads still finish normally.

```cpp
Foo foo;

std::thread tA([&foo]{
    try {
        foo.first([]{ throw std::runtime_error("printer offline"); });
    } catch (const std::runtime_error&) {
        // must land here -- the exception must propagate out of first()
    }
});
std::thread tB([&foo]{ foo.second([]{ std::cout << "second"; }); });  // must not hang
std::thread tC([&foo]{ foo.third ([]{ std::cout << "third";  }); });  // must not hang

tA.join();
tB.join();
tC.join();
```

The harness also runs the same shape with `second`'s callback throwing instead, to check
`third()` doesn't hang either.
