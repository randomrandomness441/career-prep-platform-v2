A deadlock needs a cycle: thread 1 waits on something thread 2 holds, and thread 2 waits on
something thread 1 holds. Break any link in the cycle and it cannot form.

There are two ways to break it here, and both are worth knowing:

1. Make every thread acquire the mutexes in the **same order**, so a cycle is impossible by
   construction. What property do two `Account` objects have that gives every thread the
   same answer about which comes "first"?
2. Never sit holding one lock while blocking on another. Take one, *try* the other, and if
   the try fails, drop everything and start over.

---

Option 2 is what the standard library already implements, and you do not have to write it.

```cpp
std::scoped_lock lk(from.m_, to.m_);    // C++17, locks both, deadlock-free
```

or the older spelling, which does the same thing in two steps:

```cpp
std::lock(from.m_, to.m_);                                     // acquires both safely
std::lock_guard<std::mutex> g1(from.m_, std::adopt_lock);      // "already locked; just unlock me later"
std::lock_guard<std::mutex> g2(to.m_,   std::adopt_lock);
```

`std::adopt_lock` tells the guard that the mutex is *already* held, so it should skip the
locking and only take responsibility for the unlocking. Without it you would lock twice.

Neither of these handles `transfer(a, a, n)`. Both have a precondition that the mutexes are
distinct — handing `std::lock` the same mutex twice is undefined behaviour, not an error it
detects.

---

```cpp
bool transfer(Account& from, Account& to, long amount) {
    if (&from == &to) return true;         // required: distinct mutexes
    std::scoped_lock lk(from.m_, to.m_);
    if (from.balance_ < amount) return false;
    from.balance_ -= amount;
    to.balance_ += amount;
    return true;
}
```

The hand-rolled alternative — option 1 from the first hint — orders by address:

```cpp
bool transfer(Account& from, Account& to, long amount) {
    if (&from == &to) return true;
    std::mutex* first  = &from.m_;
    std::mutex* second = &to.m_;
    if (std::less<std::mutex*>{}(second, first)) std::swap(first, second);
    std::lock_guard<std::mutex> g1(*first);
    std::lock_guard<std::mutex> g2(*second);
    ...
}
```

Both threads now lock the lower-addressed mutex first, so the cycle cannot form. Use
`std::less` rather than bare `<`: comparing pointers into unrelated objects with `<` is
unspecified, while `std::less` is guaranteed to give a total order.

Prefer `std::scoped_lock`. It is one line, it cannot be got wrong, and it does not require
every future caller in the codebase to remember the convention.
