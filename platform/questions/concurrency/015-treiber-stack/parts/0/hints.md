Start with the shape of a compare-exchange retry loop, and notice what
`compare_exchange_weak` does on **failure**: it overwrites your `expected` variable with the
value the atomic actually holds. That is not a side effect to work around — it is the retry
already being set up for you, which is why these loops have empty bodies.

```cpp
long cur = counter.load(std::memory_order_relaxed);
while (!counter.compare_exchange_weak(cur, cur + 1)) {
    // empty: cur has been refreshed
}
```

For `push`, pick your `expected` variable carefully. Which one, if it were refreshed on every
failure, would leave the new node correctly linked for the next attempt?

For `pop`, ask what the successful compare-exchange actually gives you. Exactly one thread's
exchange can unlink a given node — so after it succeeds, that node is *yours*, and no other
popper is competing for its contents. The danger is not another popper. It is the allocator.

---

Sketch out this schedule on paper before writing any code:

```
thread A:  Node* old = head_.load();          // old == X
                                              // <-- descheduled here
thread B:  pops X, and calls delete on it
thread C:  pushes a new node; the allocator hands back X's address
thread A:  ... reads old->next
```

Two separate disasters live in that gap.

- A is dereferencing memory that was freed. Whatever it reads is another node's bytes.
- Even if A somehow survives the read, `head_` is the value X again, so A's
  compare-exchange **compares equal and succeeds** — installing a `next` pointer that was
  read from a different era of the stack. That is ABA. It is not a data race, TSan will not
  see it, and it corrupts the list silently.

Both come from the same root: an address was recycled. So ask the question the other way
round — what would have to be true for a popped node's address to *never* be handed back
while the stack is alive?

The answer you need for this exercise is allowed to be blunt. It is also the answer real
implementations reject, and the reading says why.

---

Retire popped nodes onto a second lock-free list and delete them in the destructor:

```cpp
struct Node {
    T data;
    Node* next = nullptr;
    Node* retire_next = nullptr;    // a SEPARATE link — see below
};

void retire(Node* n) {
    n->retire_next = retired_.load(std::memory_order_relaxed);
    while (!retired_.compare_exchange_weak(n->retire_next, n,
                                           std::memory_order_release,
                                           std::memory_order_relaxed)) {
    }
}
```

Nothing is ever freed while the stack is live, so there is no use-after-free and no address
is ever recycled, so there is no ABA. The cost is unbounded memory growth — real
implementations use hazard pointers instead.

The separate `retire_next` field is not cosmetic. If `retire()` reused `next`, you would be
*writing* to a field that another thread's in-flight compare-exchange is still *reading*, on a
node it thinks is still in the stack. That is a genuine data race and ThreadSanitizer will
report it.

Memory orders, and what each one is for:

```cpp
void push(T value) {
    Node* n = new Node(std::move(value));
    n->next = head_.load(std::memory_order_relaxed);   // n is private; nothing to order
    while (!head_.compare_exchange_weak(n->next, n,
                                        std::memory_order_release,   // publish the node
                                        std::memory_order_relaxed)) {}
}

bool pop(T& out) {
    Node* old = head_.load(std::memory_order_acquire); // see what push published
    while (old && !head_.compare_exchange_weak(old, old->next,
                                               std::memory_order_acquire,
                                               std::memory_order_acquire)) {}
    if (!old) return false;
    out = std::move(old->data);
    retire(old);
    return true;
}
```

`release` on the successful push and `acquire` on the pop side are the pair that makes the
node's contents visible along with its pointer. Without them a popper can legally see the new
head while the bytes behind it are still uninitialised. The `old &&` short-circuit matters
too: on an empty stack `old` is null and `old->next` must not be evaluated.

`_weak` rather than `_strong` because we are already in a retry loop: a spurious failure
costs one extra pass and nothing else. On load-linked/store-conditional machines `_strong`
has to wrap the LL/SC pair in an extra hidden loop to suppress spurious failures, which is
pure waste here. (On this particular machine, which has ARMv8.1 LSE atomics, both compile to
a single `cas` instruction and are identical — but `_weak` is still the right habit.)
