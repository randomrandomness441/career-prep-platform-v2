The bug is not "the lock is missing". The lock is there three times. The bug is that the
lock is *released* twice — once after `empty()` returns and once after `top()` returns —
and each release is an invitation for another thread to change the answer you just got.

Ask: how many times should the mutex be locked and unlocked during one logical
"take the top item off"?

---

Once. So the check and the removal have to live inside the same function, inside the same
`lock_guard`.

That means `pop()` has to answer two questions at once: *was there anything there?* and
*what was it?* One return value has to carry both. `std::optional<int>` is exactly that
shape — `std::nullopt` for "nothing was there".

And `top()` has to go. If it stays, someone will call `top()` then `pop()` and reintroduce
the gap. An interface that can be used wrongly will be.

---

```cpp
std::optional<int> pop() {
    std::lock_guard<std::mutex> g(m_);
    if (data_.empty()) return std::nullopt;
    std::optional<int> result(data_.back());
    data_.pop_back();
    return result;
}
```

Note the order of the last two lines. Build the result *first*, shrink the container
*second*. If `T`'s move constructor throws, the element is still on the stack. Reverse the
two lines and a throw destroys the element with nobody holding a copy — that is the problem
part 1 is about.
