# Follow-up: Don't Lose the Element

## ELI5: dropping the slice on the way to your plate

Part 0 fixed the "two roommates, one slice" race, grabbing a slice is now one clean,
uninterruptible action. But there's a second, sneakier way to lose the slice: what if you
successfully take it *out of the box*, and then trip and drop it on the way to your
plate?

Look at what a by-value `pop()` actually has to do, in order:

```cpp
T value = std::move(data_.back());   // 1. take the slice out of the box
data_.pop_back();                    // 2. the box no longer has it
return value;                        // 3. carry it to the caller's plate
```

Between step 2 and the end of step 3, the slice exists in exactly one place: your hands,
mid-walk. If step 3 "trips" (`T`'s copy constructor allocates memory and that allocation
fails, or `T` is some type whose copying can throw), the slice you were carrying is
gone. Not back in the box, not on anyone's plate: gone, destroyed while your hands
(a local variable, mid-return) get cleaned up during the crash. The box doesn't have it,
the caller doesn't have it, nobody anywhere is holding a copy to try again with.

Anthony Williams' answer to this: design `pop()` so that it never does anything with `T`
that's risky enough to trip on, at the exact moment the slice would otherwise be lost.

## Your task

Implement `threadsafe_stack<T>` with two `pop()` overloads, neither of which can lose an
element:

```cpp
template <typename T>
class threadsafe_stack {
public:
    threadsafe_stack() = default;
    threadsafe_stack(const threadsafe_stack&) = delete;
    threadsafe_stack& operator=(const threadsafe_stack&) = delete;

    void push(T value);

    // Returns the top element, or nullptr if the stack is empty.
    std::shared_ptr<T> pop();

    // Copies the top element into `out`. Returns false if the stack is empty.
    bool pop(T& out);

    bool empty() const;
};
```

## Requirements

1. Everything part 0 required still holds: `pop()` is one atomic operation, and
   concurrent poppers never see the same element twice.
2. **No element may be destroyed by a failing copy.** If a copy or move of `T` throws
   anywhere inside `pop()`, the exception may propagate, but afterwards the element must
   still be on the stack, so the caller can just try again.
3. The same holds for `pop(T& out)`. If writing into `out` throws, the element stays put.
4. `push()` may throw. That's fine: the caller still owns the value they tried to push,
   so nothing is lost either way.

## Why the constraints exist

- **One `std::mutex`.** Same as part 0, this is about interface design and exception
  safety, not new synchronization primitives.
- **The tests use a `T` whose copy and move constructors throw on a schedule you don't
  control**, on purpose, so you can't get lucky. Think about which of your lines
  actually invoke a copy or move, and whether the element is still safely in the
  container at the moment each one runs.
