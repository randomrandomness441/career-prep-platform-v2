# Race Conditions Inherent in Interfaces

## ELI5: the last slice of pizza

There's one slice of pizza left in the box, and two roommates both want it. Each of them,
separately, does the polite thing. They open the box, check "is there a slice in there?",
see yes, and reach in to take it.

The problem sits *between* "check" and "take." Roommate A opens the box and sees the
slice. A half-second before they actually grab it, roommate B also opens the box and also
sees the slice. It's still there because A hasn't taken it yet. B also decides to grab it.
Now they both reach in. One of them gets the slice. The other one's hand closes on an
empty box, even though they *definitely* checked first and *definitely* saw a slice.

Nobody did anything individually wrong. Checking, then taking, is exactly how you'd
naturally act. The bug isn't in either step. It's in the gap *between* the two steps,
where the world can change out from under you.

## What's already been tried

Here's a stack that a careful person wrote. Every method locks the mutex. No step is
individually unsafe. ThreadSanitizer finds nothing wrong with it:

```cpp
class threadsafe_stack {
    mutable std::mutex m_;
    std::vector<int> data_;
public:
    void push(int v) { std::lock_guard g(m_); data_.push_back(v); }
    bool empty() const { std::lock_guard g(m_); return data_.empty(); }
    int top() const { std::lock_guard g(m_); return data_.back(); }
    void drop() { std::lock_guard g(m_); data_.pop_back(); }
};
```

And yet it's unusable, because the only way to actually take something off it is this:

```cpp
if (!s.empty()) { // true, "is there a slice?"
    int v = s.top(); // another thread popped in this gap, "let's grab it"
    s.drop();
}
```

That's the pizza box, exactly. Both threads see a stack with one element. Both read that
element. Both remove one. One value comes out twice, and one is lost forever. No
individual line is wrong. The *combination* is the problem. No amount of locking inside
the existing methods can fix a gap between two separate method calls.

## Your task

Implement `threadsafe_stack` with an interface that physically cannot be misused this way.
There's no "check, then act" as two separate steps at all:

```cpp
class threadsafe_stack {
public:
    threadsafe_stack() = default;
    threadsafe_stack(const threadsafe_stack&) = delete;
    threadsafe_stack& operator=(const threadsafe_stack&) = delete;

    void push(int value);

    // Removes and returns the top element, or std::nullopt if the stack is empty.
    std::optional<int> pop();

    // Advisory only.
    bool empty() const;
};
```

## Requirements

1. `pop()` is a single atomic operation: the emptiness check and the removal happen
 without releasing the lock in between. "Is there a slice, and if so give it to me"
 has to be one indivisible action, not two.
2. `pop()` reports emptiness through its return value, so a caller never needs to ask
 "is there one?" as a separate question first.
3. **There is no `top()`.** A value you can look at without taking it is a value another
 thread may already be reaching for.
4. LIFO order holds when only one thread is using the stack.
5. `pop()` on an empty stack returns `std::nullopt`. It does not throw and does not have
 undefined behaviour.

## Why the constraints exist

- **One `std::mutex`.** This is not a lock-free exercise. The fix here is entirely about
 interface design, not synchronization primitives.
- **`empty()` may stay, but only as advisory.** The moment you read its answer, it can
 already be stale, like glancing at the pizza box from across the room. Callers must
 not build real decisions on it.
