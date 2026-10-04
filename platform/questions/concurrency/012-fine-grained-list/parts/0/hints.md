Start with the node and the walk, not with removal.

```cpp
struct node {
    std::mutex m;
    std::optional<int> value;    // empty only for the dummy head
    std::unique_ptr<node> next;
};
node head_;                       // the dummy
```

The walk needs two `std::unique_lock`s alive at once, because `lock_guard` cannot be
unlocked early or moved. Write the loop so that the lock on the next node is taken while
the lock on the current node is still held, and only then release the current one.

Ask yourself at each step: *if I let go here, what could change before I look again?*
---
The walk, and then the removal.

```cpp
node* current = &head_;
std::unique_lock<std::mutex> lk(head_.m);
while (node* const next = current->next.get()) {
    std::unique_lock<std::mutex> next_lk(next->m);   // grab the next rung
    lk.unlock();                                     // then let go of this one
    f(*next->value);
    current = next;
    lk = std::move(next_lk);
}
```

`find_first_if` is the same loop with a `return` in it — copy the value out while the node is
still locked.

For `remove_if`, do **not** unlock `current`. You are about to write `current->next`, so you
must hold `current->m` for that write, and you must hold `next->m` so that no walker is
parked inside the node you are about to destroy.

Then think about the order of two things at the end: releasing the victim's mutex, and
running the victim's destructor. One of those orders destroys a locked mutex.
---
```cpp
void remove_if(const std::function<bool(int)>& pred) {
    node* current = &head_;
    std::unique_lock<std::mutex> lk(head_.m);
    while (node* const next = current->next.get()) {
        std::unique_lock<std::mutex> next_lk(next->m);
        if (pred(*next->value)) {
            std::unique_ptr<node> doomed = std::move(current->next);
            current->next = std::move(next->next);
            next_lk.unlock();          // release BEFORE `doomed` is destroyed
            // `doomed` dies at the end of this block. We still hold current->m,
            // and reaching `next` requires current->m, so nobody can be inside it.
        } else {
            lk.unlock();
            current = next;
            lk = std::move(next_lk);
        }
    }
}
```

Note that `current` does not advance when a node is removed — the new `current->next` is the
node after the one you just deleted, and it still needs testing.

Two details that are easy to miss:

- `push_front` only ever takes `head_.m`, and it must take it *after* the new node is
  allocated and the value copied. Allocation under a lock is wasted lock time.
- The destructor is `remove_if([](int) { return true; })`. Letting the default
  destructor run a chain of `unique_ptr`s recurses once per element and overflows the stack
  on a long list.

Why plain `delete` is enough here, with no reference counting: a thread can only attempt to
lock `next->m` while it already holds `current->m`, and the remover holds `current->m`. So at
the moment of destruction there is provably nobody inside the node and nobody blocked on its
mutex. That single fact is what fine-grained *reading* buys you on the *writing* side, and it
is why letting go of the predecessor's lock breaks everything at once.
