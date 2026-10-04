## 1. Reframe the problem

A mutex is not a thing that protects an object. It is a thing that protects an *invariant*,
and the size of the mutex you need is the size of the invariant you are breaking.

Wrapping a whole list in one mutex says: "the invariant is the entire list, and while I am
touching it nobody may look at any part of it." That is true for a `push_front`. It
rewrites the head pointer, and while `head.next` is half-written the list is not a list.
But it is enormously untrue for a walk, which reads one node at a time and never breaks
anything at all. The one big mutex is not wrong. It is a claim about the invariant that is
far larger than the truth, and you pay for the gap in throughput.

So the question is: what is the smallest thing that has to be intact for a walk to be safe?

A walk is standing on node `A` and about to follow `A->next` to node `B`. The only thing
that can hurt it is somebody removing `B` and freeing it in between reading the pointer and
arriving. Removing `B` means rewriting `A->next`. So the invariant a walker needs is exactly
one link: *the pointer in the node I am standing on stays valid while I am standing here.*

Give every node its own mutex and that becomes expressible. To move from `A` to `B`, lock
`B` **before** releasing `A`. For a moment you hold both, and during that moment no remover
can rewrite `A->next` (it would need `A`) and none can free `B` (it would need `B`). Then you
let go of `A` and walk on. Two locks at a time, always taken front to back, never the other
way, so no two walkers can form a cycle. That is **hand-over-hand** locking, also called
lock coupling, and it is exactly how you climb a ladder: never let go of one rung until the
next one is in your hand.

Everything else in the exercise is a consequence. Removal needs two locks because it
rewrites one node's pointer and destroys another. A dummy head node exists so that the first
real element has an ordinary predecessor with an ordinary mutex, which is what removes every
special case from `push_front` and from removing the first element.

## 3. The broken version, first

### The version that is merely slow

```cpp
void for_each(const std::function<void(int)>& f) {
    std::lock_guard<std::mutex> lk(list_m_);
    for (node* cur = head_.next; cur; cur = cur->next) f(*cur->value);
}
```

Nothing is wrong with this. It is correct on any schedule. It is also a promise that only
one thread will ever be inside the list, and the cost of that promise is not subtle.

Four threads, each walking a 200-node list 40 times, with about 3.6 µs of work per element
in the callback, a callback that does something, which is the only reason `for_each` exists:

```
40 walks of a 200-node list per thread

threads | one big mutex | hand over hand
      1 |      29.28 ms |      29.14 ms
      2 |      58.57 ms |      29.37 ms
      4 |     116.48 ms |      29.28 ms
      8 |     233.64 ms |      36.20 ms
```

The big-mutex column doubles every time the thread count doubles. Eight threads take eight
times as long as one, on a machine that has ten cores and is using one of them. The
hand-over-hand column is flat. The walkers pipeline down the list, each one a few nodes
behind the last, so adding threads adds throughput instead of queue.

That is the entire argument for this question. Now the interesting part, which is what
happens when you try to get there.

### The version that is actually broken

The natural first move is to leave the reads alone and make *removal* fine-grained, because
removal is the operation that felt like it deserved a lock in the first place:

```cpp
void remove_if(const std::function<bool(int)>& pred) {
    node* prev = &head_;
    while (node* const cur = prev->next) {
        std::unique_lock<std::mutex> cur_lk(cur->m);   // lock the node I am deleting
        if (pred(*cur->value)) {
            prev->next = cur->next;
            cur_lk.unlock();
            delete cur;
        } else {
            prev = cur;
        }
    }
}
```

Read that and it feels careful. It locks the node it is about to destroy. What it does not
lock is `prev`, and `prev->next` is the pointer everybody else follows to get here.

Two pushers, one remover and one walker, on a list of 600 elements:

```
--- run 1 ---
476 walks while the remover ran
elements left: 388, expected 400
exit=0
--- run 2 ---
2760 walks while the remover ran
elements left: 399, expected 400
exit=0
--- run 3 ---
2914 walks while the remover ran
elements left: 400, expected 400
exit=0
--- run 4 ---
0 walks while the remover ran
elements left: 400, expected 400
exit=0
--- run 5 ---
1251 walks while the remover ran
elements left: 397, expected 400
exit=0
--- run 6 ---
1697 walks while the remover ran
elements left: 392, expected 400
exit=0
```

Twelve elements gone in run 1, one in run 2, none in runs 3 and 4, three in run 5, eight in
run 6. Those elements were pushed successfully and never matched the removal predicate. They
are gone because `prev->next = cur->next` and `head_.next = fresh` are two writes to the same
word, one of them under `list_m_` and one of them under nothing, so whichever lands second
erases the other.

An earlier batch of the same program did something worse: one run printed nothing at all and
had to be killed after two minutes. That is the `delete cur` half of the bug. A walker standing on `prev` read a pointer to
`cur`, the remover freed `cur`, the allocator handed that same block back to a pusher as a
brand new node somewhere near the front of the list, and the walker followed the pointer into
it and started going round a loop that has no end.

Note what the run-to-run spread means: **three of those six runs looked fine.** A test that
ran the scenario once and checked the count would have passed. This is why the harness for
this question runs many trials and checks three separate things: that a walk terminates,
that a walk never sees the same value twice, and that the final contents are exactly right.

ThreadSanitizer names it precisely:

```
WARNING: ThreadSanitizer: data race
  Read of size 8 by thread T2:
    #0 threadsafe_list::remove_if(std::function<bool(int)> const&) solution.hpp:39
  Previous write of size 8 by thread T1 (mutexes: write M0):
    #0 threadsafe_list::push_front(int) solution.hpp:33
SUMMARY: ThreadSanitizer: data race solution.hpp:39 in threadsafe_list::remove_if
```

Line 33 is `head_.next = fresh;`, held under `list_m_`, note the `(mutexes: write M0)`.
Line 39 is `while (node* const cur = prev->next)`, held under nothing. A mutex only
synchronises the accesses that all agree to take it.

### The fix

Hold both locks across the unlink, and only let go of the victim while still holding its
predecessor:

```cpp
std::unique_lock<std::mutex> lk(head_.m);              // the predecessor's lock
while (node* const next = current->next.get()) {
    std::unique_lock<std::mutex> next_lk(next->m);     // the victim's lock
    if (pred(*next->value)) {
        std::unique_ptr<node> doomed = std::move(current->next);
        current->next = std::move(next->next);
        next_lk.unlock();
        // doomed dies here: its mutex is released, and we still hold `current`,
        // so nothing can reach it and nothing is parked inside it.
    } else {
        lk.unlock();
        current = next;
        lk = std::move(next_lk);
    }
}
```

The reason this is safe is worth saying out loud, because it is the only argument in the
whole design that is not obvious. Freeing a node while another thread is *blocked on its
mutex* would be catastrophic. You would destroy a locked mutex under someone's feet. It
cannot happen here: to attempt to lock `next->m`, a thread must already hold `current->m`,
and the remover holds `current->m`. So the remover knows, without any reference counting or
deferred reclamation, that nobody is in the node and nobody can enter it. Hand-over-hand
locking is not just how you walk the list safely. It is also what makes deletion safe with
plain `delete`.

## 6. Where this solution fails

- **It is slower than one big mutex whenever the callback is cheap.** Same benchmark, same
  list, but with about 2.6 ns of work per element instead of 3.6 µs, a callback that does
  essentially nothing:

  ```
  400 walks of a 200-node list per thread

  threads | one big mutex | hand over hand
        1 |       0.21 ms |       0.57 ms
        2 |       0.58 ms |       4.98 ms
        4 |       2.11 ms |       8.86 ms
        8 |       2.36 ms |      21.19 ms
  ```

  Nine times *worse* at eight threads. Two lock/unlock pairs per element is a lot of work
  when visiting an element is not, and all the walkers enter at the head, so they queue on
  the same first few mutexes instead of spreading out. Fine-grained locking is a trade of
  constant factor for parallelism. If there is no real work to parallelise, you have paid the
  constant factor for nothing. Measure before you reach for this.

- **Every node carries a mutex.** A `std::mutex` is 64 bytes on this platform. A list of
  `int` goes from roughly 24 bytes a node to roughly 96, and a walk touches four times as
  many cache lines. For small elements the memory cost alone can outweigh the concurrency
  gain.

- **`for_each` gives you no snapshot.** It sees the list as it is at each step. An element
  pushed while you are past its position is missed. An element removed behind you was still
  reported. `find_first_if` returns a value that may already have been removed by the time
  it lands in your hand. There is no consistent view here and there cannot be one. The whole
  design is built on never holding more than two locks, and a snapshot needs all of them.

- **`size()` is not implementable.** Any count you produce was true at no single moment. Same
  for "is it empty." This is not an omission from the interface. It is the same interface race
  as a thread-safe stack's `empty()` and `pop()`, and adding the method would only move the
  bug into the caller.

- **A slow callback blocks the node.** `f` runs while the node's mutex is held, so a callback
  that does I/O, or that takes another lock, stalls every walker behind it and every remover
  that wants that node. Worse, a callback that calls back into the list, `f` doing another
  `for_each`, say, deadlocks instantly, because it will try to take `head_.m` from the head
  while holding a node further down. The rule "never call unknown code while holding a lock"
  is violated by design here, and the price is that the callback must be trusted, fast, and
  must not touch the list.

- **`push_front` contends on exactly one mutex.** Every push takes `head_.m`, so writes do
  not scale at all. Only walks do. And a walker at the head holds `head_.m`, so a long list
  full of readers still serialises pushers behind the first node. A push-heavy workload gets
  nothing from this design.

- **Insertion anywhere but the front, and removal, meet at the same two locks.** Two removers
  working on adjacent nodes serialise. A remover deleting several nodes in a row holds a lock
  for the whole run. The parallelism is real but it is only as wide as the list is long.

- **Traversal order is the only lock order, and it must be obeyed everywhere.** Any operation
  that ever locks a later node before an earlier one, a `reverse()`, a `swap` of two
  elements, a `merge` of two lists, reintroduces the cycle that this design carefully
  avoids. The safety argument is global, not local, and a single new method can destroy it.

- **The destructor is not thread-safe, and cannot be.** Destroying the list while any thread
  is inside it is undefined behaviour, and no per-node locking can fix that: the mutexes
  themselves are being destroyed. All threads must be joined before the list dies, which is
  an ownership discipline you enforce outside the class.

## 7. Interview follow-ups

**"Why is it safe to delete a node with plain `delete` here, with no reference counting or
hazard pointers, doesn't a concurrent reader risk a use-after-free?"** The safety argument
is entirely structural, not runtime-checked. To reach `next->m` at all, a thread must already
hold `current->m`, and the remover, deleting `next`, holds `current->m` for the whole
operation. So while the remover is deleting, no other thread can be blocked trying to enter
that node (they'd need the lock the remover already has), and no other thread is already
inside it either (same reason). Hand-over-hand locking doesn't just make walking safe. It's
what lets deletion skip the reclamation machinery [[015-treiber-stack]] and
[[040-lock-free-hashmap]] both need, because the lock ordering itself proves nobody's
looking.

**"You measured fine-grained locking being 9x WORSE than one big mutex when the callback
does almost nothing. Why does the 'better' design lose so badly there?"** Fine-grained
locking trades a constant-factor cost, two lock/unlock pairs per node visited versus one
lock for the whole walk, for the ability to parallelize real work. When there's no real work,
a 2.6ns callback, that constant factor is the entire cost, and paying it twice per node
for nothing measurably loses. Worse, every walker enters at the head and queues on the same
first few mutexes rather than spreading out, so the parallelism this design exists to buy
doesn't even materialize at the entry point. The lesson generalizes past this one list: never
adopt a finer-grained locking scheme without measuring the actual per-operation cost against
what you're protecting.

**"Small machine, 2 cores, several threads walking the list. Does hand-over-hand still pay
off over one big mutex?"** The measured 200-node, 3.6µs-per-element benchmark shows the
crossover happening even at 2 threads (58.57ms single-mutex vs 29.37ms hand-over-hand,
already flat), but that flat curve depends on genuine parallelism existing to exploit. With
only 2 cores, the ceiling on how much concurrent walking is possible is lower, so the
absolute win shrinks even though the qualitative shape (single mutex scaling linearly worse,
hand-over-hand staying flat) holds regardless of core count.

**"A caller wants find_first_if to guarantee the returned value is still in the list by the
time they use it, can you support that?"** Not with this design's own consistency model.
`find_first_if` (like `for_each`) only ever guarantees the list looked a certain way at the
instant each node was visited, and by the time a result reaches the caller's hand, a remover
that was never holding more than two locks at once could already have removed it. This is
structurally the same interface race [[004-interface-races]]'s reading covers for a
thread-safe stack's `top()`, a "check, act later" pattern the class's own atomic operations
can't close on the caller's behalf. Supporting a real guarantee would need the caller to hold
the specific node's lock across both the lookup and the use, which this API doesn't expose,
and exposing it would let a caller hold a lock indefinitely, stalling every walker and
remover behind it.

**"You mentioned a reverse() or merge() operation could break the whole safety argument,
why, specifically?"** The entire correctness proof rests on one fact: every lock acquisition
in the whole program proceeds strictly front-to-back along the list. A `reverse()` or a
`merge()` of two lists, by its nature, needs to touch nodes in an order that isn't simply
"the next one after where I currently am." Any operation that locks a *later* node before an
*earlier* one (relative to the list's traversal order) reintroduces exactly the cyclic
lock-acquisition shape [[005-scoped-lock]]'s bank-transfer deadlock demonstrates, just spread
across more than two mutexes. The safety argument here is global, every operation, everywhere
in the codebase, obeys the same order, not local to any one method. That means a single
new method that doesn't respect it can silently reintroduce deadlock into code that was
correct before it was added.
