There is exactly one dangerous window: the stretch of time when the element has been
removed from the container but has not yet reached the caller. Any throw in that window
destroys it.

Two ways to close a window: make it shorter, or make nothing that can throw happen inside
it. Only the second one is a guarantee.

---

So: what has to be true for `pop()` to contain no throwing operation at all?

It cannot construct a `T`. Not a copy, not a move. Which means the object the caller
receives must already exist, fully constructed, before `pop()` is entered.

Where could it have been constructed? The only other entry point is `push()`. And a throw
during `push()` is harmless — the caller still holds their own value and the container was
never modified.

What can you store in the container so that "handing it over" is a pointer operation
instead of a `T` operation?

---

Store `std::shared_ptr<T>`, allocated in `push()`:

```cpp
std::vector<std::shared_ptr<T>> data_;

void push(T value) {
    std::shared_ptr<T> p = std::make_shared<T>(std::move(value));  // may throw — harmless
    std::lock_guard<std::mutex> g(m_);
    data_.push_back(std::move(p));
}

std::shared_ptr<T> pop() {
    std::lock_guard<std::mutex> g(m_);
    if (data_.empty()) return nullptr;
    std::shared_ptr<T> result = std::move(data_.back());   // pointer move, noexcept
    data_.pop_back();                                      // destroys a null pointer
    return result;                                         // another pointer move
}
```

`T`'s copy constructor is never called. There is nothing left that can throw.

For the out-parameter overload the same principle applies from the other side: the caller
already owns the storage, so do the risky part **first**.

```cpp
bool pop(T& out) {
    std::lock_guard<std::mutex> g(m_);
    if (data_.empty()) return false;
    out = *data_.back();   // if this throws, the next line never runs
    data_.pop_back();
    return true;
}
```

Use a copy, not `std::move`. A move-assignment that throws partway leaves the element on
the stack in a moved-from state — a quieter version of the same loss.
