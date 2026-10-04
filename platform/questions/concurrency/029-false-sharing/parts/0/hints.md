A plain array `std::atomic<long> slots_[N];` packs 8 consecutive counters into one 64-byte
cache line (`sizeof(long) == 8`). That satisfies requirement 1 but not requirement 2 — it's
exactly the layout the exercise is asking you to avoid.
---
Wrap each counter in its own struct and align *that struct* to 64 bytes, then make an array
of the struct:

```cpp
struct alignas(64) Slot { std::atomic<long> v{0}; };
Slot slots_[N];
```

`alignas(64)` on the struct forces every `Slot` — including each element of the array — to
start on a 64-byte boundary. Because the struct's only member is 8 bytes, the compiler pads
`sizeof(Slot)` up to 64 as well, so consecutive slots are automatically spaced 64 bytes
apart with nothing in between wasted for anything else to alias.
---
`address_of(i)` should just be `reinterpret_cast<std::uintptr_t>(&slots_[i].v)` (or
`&slots_[i]` if you're padding a plain `long` instead of an atomic — either works as long
as the alignment holds). Don't overthink the rest: `increment`/`get` are the same one-line
bodies you'd write with no padding at all. The only thing that changes between the broken
and the correct version is the layout of `slots_`.
