# Print FooBar Alternately

## ELI5: two chanters, strict turn order

Two people are chanting, back and forth: one always says "foo," the other always says
"bar," and it has to come out `foo bar foo bar foo bar ...`, never `bar` before its
matching `foo`, never two `foo`s in a row. They were told to start whenever they feel
like it, the "go" signal for each of them arrives in whatever order the world happens to
deliver it, but the chant itself still has to come out perfectly interleaved.

## What you're actually building

Two threads share one `FooBar` object, constructed with a count `n`.

- Thread A calls `foo()` once. It must print `"foo"` `n` times.
- Thread B calls `bar()` once. It must print `"bar"` `n` times.

```cpp
class FooBar {
public:
    FooBar(int n);
    void foo(std::function<void()> printFoo);
    void bar(std::function<void()> printBar);
};
```

For `n = 3` the only acceptable output is `foobarfoobarfoobar`.

## Why the constraints exist

- **`foo` goes first, always.** `bar` must never print before its matching `foo`.
- **No busy-waiting.** A thread that can't proceed yet has to actually sleep, not spin in
 a loop asking "my turn?"
- **Both calls must return after exactly `n` prints**, nobody's left mid-chant, asleep,
 when the caller tries to join both threads.
