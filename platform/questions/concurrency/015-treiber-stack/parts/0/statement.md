# Treiber Stack, Lock-Free push and pop

## ELI5: a spring-loaded plate dispenser, no line required

Picture the spring-loaded plate dispenser at a busy cafeteria. Push a plate down, it's on
top. Grab the top plate, the spring pushes the next one up. Nobody has to form a queue to
use it, if two people reach for it at the exact same instant, one of them just... tries
again a split second later. That's the whole appeal of a *lock-free* stack: instead of a
line where everyone waits their turn (a mutex), everyone just reaches in, and if it turns
out someone beat them to it, they retry. Nobody ever has to stand there blocked.

A stack is the easiest structure to build this way, because the entire thing is defined
by one piece of information: what's currently on top. Every change is one
"swap the top, but only if it's still what I think it is" move (a compare-and-swap), and
if that check fails because someone else changed it first, you just try again.

## Where the obvious version breaks

Here's the version everyone writes first:

```cpp
void push(T value) {
    Node* n = new Node(std::move(value));
    n->next = head_.load();
    head_.store(n);
}

bool pop(T& out) {
    Node* old = head_.load();
    if (!old) return false;
    head_.compare_exchange_strong(old, old->next);
    out = std::move(old->data);
    delete old;
    return true;
}
```

`head_` is a `std::atomic<Node*>`, so nothing here can tear and there's no data race on
the pointer itself. It's still wrong three separate ways:

- **Two pushes that both look at the same "current top" both store their own plate on
 it.** One of the two plates, and everything under it, vanishes from the stack.
- **`pop` throws away whether its swap actually succeeded.** If it failed, the top never
 changed, but this code hands back `old->data` anyway, so two people can walk away with
 the same plate.
- **`delete old` throws away a plate another thread might be mid-reach for**, in the gap
 between that thread's own read of "what's on top" and its own next step. That's not a
 lost plate, it's a hand closing on one that's already been thrown in the trash, a
 crash, not a wrong answer.

## Your task

Implement a lock-free stack that survives all three:

```cpp
template <typename T>
class LockFreeStack {
public:
    LockFreeStack();
    ~LockFreeStack();
    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;

    void push(T value);

    // Moves the top element into `out` and returns true.
    // Returns false if the stack was empty at the moment we looked.
    bool pop(T& out);

    bool empty() const;
};
```

## Requirements

1. **No mutex, no locks, anywhere.** `push` and `pop` must be compare-exchange retry
 loops on the head pointer, reach in, check, retry if you lost the race.
2. **Every pushed value comes out exactly once.** Not lost, not duplicated. The tests run
 four producers and four consumers and tally every value.
3. **No use-after-free.** A node must not be freed while another thread could still be
 reaching for it. You *are* allowed to solve this by not freeing popped nodes until the
 destructor, but then say so, and know what it costs. (The reading explains why the
 real answers are hazard pointers or epoch-based reclamation, and why they're out of
 scope here.)
4. **No ABA corruption.** If your design lets a node's address get handed back out while
 another thread is still holding a stale copy of it, a compare-exchange can compare
 equal against a node that isn't actually the one you saw, and splice the list wrongly.
5. **Clean under ThreadSanitizer.** Getting the right answer isn't enough, the memory
 orders have to actually establish happens-before between the thread that writes a node
 and the thread that reads it.
6. The destructor may assume it runs single-threaded, and must not leak.

## Why the constraints exist

- **`head_` must be a `std::atomic<Node*>`.** Don't hide a mutex inside the node or the
 class, that would just be a disguised lock, defeating the whole point.
- **Use explicit memory orders.** `seq_cst` everywhere will pass the tests, but state
 which order each operation actually needs and why, that reasoning is the actual skill
 being tested here.
- **`pop` on an empty stack returns `false`.** It must not throw and must not block.
