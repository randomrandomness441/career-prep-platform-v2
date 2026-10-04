# False Sharing (Cache-Line Padding)

## ELI5: two coworkers, two separate in-trays, one wobbly shared stand

Two coworkers each have their own in-tray for paperwork, they never touch each other's
tray, never even look at each other's paperwork. But both trays happen to sit on the same
small wobbly stand. Every time coworker A drops a paper into their tray, the whole stand
jiggles, and coworker B's tray, and everything in it, jiggles too, even though B never
touched anything. Nothing gets lost or corrupted; it's just... a constant, pointless
disturbance neither of them asked for, purely because of how the furniture happens to be
arranged.

A CPU cache does something structurally identical. It doesn't move data one byte at a
time, it moves fixed 64-byte chunks called *cache lines*. Two totally independent
counters, owned by two totally independent threads on two totally different cores, can
still end up sharing one cache line if they happen to sit close together in memory. Every
write from one core silently kicks the *whole line*, including the counter it never even
touches, out of the other core's cache. Nothing about the program's correctness changes.
Its throughput falls off a cliff, for a reason that's invisible if you only read the code.

## What you're actually building

`CounterBank<N>`, `N` independent counters, one per thread, each thread only ever
touching its own counter:

```cpp
template <std::size_t N>
class CounterBank {
public:
    CounterBank() noexcept;

    void increment(std::size_t i) noexcept; // counter i += 1
    long get(std::size_t i) const noexcept; // current value of counter i

    // address of counter i's storage, as an integer -- exists so the test can
    // check the memory layout. Not something real code would normally expose.
    std::uintptr_t address_of(std::size_t i) const noexcept;
};
```

## Requirements

1. `increment(i)` and `get(i)` behave like a plain counter, after `T` threads each call
 `increment(i)` on their own `i`, `get(i)` returns the number of calls, every time.
2. **No two counters may share a CPU cache line.** A cache line on every machine this
 course targets is 64 bytes. `address_of(i)` and `address_of(j)` must fall in different
 64-byte-aligned blocks for every `i != j`, give every coworker their own stand.
3. Each thread touches only its own index. You don't need to make `increment` safe
 against two threads calling it with the *same* `i` concurrently.

## Why the constraints exist

- **`N` is a compile-time constant (template parameter)**, so you can size storage for
 it directly, no heap allocation required.
- **Do not change the public signatures above.**
