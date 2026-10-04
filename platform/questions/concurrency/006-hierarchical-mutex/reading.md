## 1. Reframe the problem

The previous question ended with a fix that only works when you can see the problem.
`std::scoped_lock(a, b)` takes two mutexes without deadlocking, but you have to be
*holding both names in one expression* for it to do anything. It is a local fix for a local
mistake.

Now look at the shape that actually deadlocks large programs:

```cpp
void cache_lookup() { void store_flush() {
        lock(cache_m); lock(store_m);
        store_read(); // ----> cache_invalidate();
    } }
```

`cache_lookup` locks the cache and calls down into storage, which locks storage. Read each
function on its own and both are flawless: one mutex, taken and released, nothing to
review. **Neither function can see two locks, so `scoped_lock` has nothing to fix.** The
cycle only exists in the call graph, cache-above-storage in one path, storage-above-cache
in the other, and no single place in the source contains it.

That reframes the problem. You are not trying to lock two things safely; you are trying to
enforce a *global* ordering rule with only *local* information. The rule you want is
"storage is below the cache, so nobody who holds a storage lock may reach up and take a
cache lock", and it has to be enforced at every lock site without any lock site knowing
what the others do.

The trick is to stop tracking mutexes and track a single number instead. Give each mutex a
level. Give each **thread** one variable saying how far down it has already descended.
A lock is legal only if it goes strictly downward. That comparison is local, one integer,
in thread-local storage, no synchronisation, and yet it enforces a property of the whole
program, because a cycle in the call graph would require some thread to step upward, and
that is precisely what the comparison forbids.

And the payoff is bigger than not deadlocking. A deadlock needs the bad *interleaving* to
show up before you learn anything. A hierarchy violation is detected the first time one
thread walks that code path, alone, on an idle machine.

## 3. The broken version, first

Two subsystems, one mutex each, locked across a call boundary, no `scoped_lock` in sight
because there is nowhere to put one:

```cpp
std::mutex cache_m, store_m;

void store_read() { std::lock_guard<std::mutex> lk(store_m); }
void cache_invalidate() { std::lock_guard<std::mutex> lk(cache_m); }

void cache_lookup() { // cache -> storage
    std::lock_guard<std::mutex> lk(cache_m);
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    store_read();
    ++lookups;
}

void store_flush() { // storage -> cache
    std::lock_guard<std::mutex> lk(store_m);
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    cache_invalidate();
    ++flushes;
}
```

Thread A runs `cache_lookup` 500 times, thread B runs `store_flush` 500 times. Actual
output:

```
after 2 seconds: 0 lookups, 0 flushes (500 each expected)
STUCK - A holds cache_m and wants store_m; B holds store_m and wants cache_m
```

Zero and zero. The process is alive and will never do anything again.

Two things to notice. First, about the 50µs sleep: it makes the deadlock fire on the first
iteration, but it is not what causes it. Removing it entirely still deadlocked on 20 runs
out of 20 here, just a few iterations later, one run got 11 flushes in before it stuck.
That is because this demo does nothing but hammer the two paths in a tight loop on ten real
cores. Production code does not hammer; the two paths meet rarely, and then the window
really is nanoseconds wide, which is how this survives CI and stops the service at 3am.

Second, and worse: **there is no line to blame.** In the previous
question the two `lock_guard`s sat next to each other and a careful reviewer could spot the
inversion. Here the reviewer has to hold the entire call graph in their head. Nobody does
that at scale, which is why "just be careful about lock ordering" is not a strategy.

Now change the two mutex types and nothing else. `cache_m` becomes level 10000, `store_m`
level 5000, the cache is above storage, so descending means cache first:

```cpp
hierarchical_mutex cache_m(10000);
hierarchical_mutex store_m(5000);
```

Same program, same threads, same 50µs sleeps. Actual output:

```
B threw: mutex hierarchy violated
after joining: 500 lookups, 0 flushes
```

`store_flush` took the 5000 lock and then reached up for the 10000 lock. That is upward, so
`lock()` threw before blocking, and thread B unwound. Thread A, whose ordering was legal,
finished all 500 lookups and the program exited normally in milliseconds.

The output is a bug report: which thread, which call path, at the exact line that broke the
rule. Compare with the first run, where all you get is a process that is not moving and a
debugger session to figure out why.

One more property worth being explicit about: **this fires even without the race.** Run
`store_flush` on its own, single-threaded, with no other thread in the program at all, and
it still throws. The check inspects the *order this thread took locks in*, not the timing
between threads. That is what turns a scheduling-dependent hang into a deterministic
unit-test failure.

## 6. Where this solution fails

- **It only checks paths that actually run.** This is a runtime assertion, not a proof. An
 inversion in an error-handling branch nobody exercises is still there, waiting. The
 hierarchy raises your odds enormously, one execution of the path is enough, no
 interleaving required, but "no violation observed" is not "no violation exists".

- **It cannot express "lock these two together".** Two mutexes at the same level are a
 violation by construction, so `std::scoped_lock(a, b)` over two same-level
 `hierarchical_mutex`es throws instead of using the deadlock-free back-off algorithm. The
 transfer-between-two-accounts shape from the previous question has *no* valid level
 assignment, because both accounts are genuinely peers. The two techniques cover different
 cases and a real system needs both: `scoped_lock` for peers taken together, a hierarchy
 for layers taken across calls.

- **Assigning the numbers is a design problem, and sometimes the answer is "you can't".**
 If component X must lock Y and Y must lock X, no numbering exists, the hierarchy is not
 failing, it is reporting a cyclic design that must be broken. Leave large gaps (10000,
 5000, 1000) so a new layer can be inserted without renumbering the world, and expect the
 numbers to end up in one header that everybody includes.

- **Mixed lock types are invisible.** Only `hierarchical_mutex` participates. One plain
 `std::mutex` left in the middle of the layering, or a lock taken inside a third-party
 library, and the cycle can reform through the gap without a single check firing.

- **It says nothing about deadlocks that are not lock-ordering deadlocks.** Thread A
 waiting on a `future` that thread B will only set after acquiring a lock A holds; two
 threads joining each other; a condition variable whose predicate can never become true.
 All still hang, silently.

- **`unlock()` trusts you to unwind in order.** It restores the value this mutex saved, so
 `a.lock(); b.lock(); a.unlock(); b.unlock();` leaves the thread's current level wrong,
 and then permits locks it should reject, or rejects locks it should permit. The
 implementation cannot tell, because the saved values form a stack that only the unlock
 order reconstructs. Using `lock_guard`/`unique_lock` makes this structurally impossible,
 which is a good argument for never calling `lock()` by hand.

- **No recursion, ever.** Locking the same `hierarchical_mutex` twice on one thread is a
 violation (equal levels), so a call path that re-enters a locked component throws. With a
 plain `std::recursive_mutex` it would have worked. That is usually the right answer,
 re-entrant locking hides broken invariants, but it does mean the class cannot be dropped
 into code that relies on recursion.

- **The saved level lives in the mutex, which only works for exclusive ownership.** One
 `previous_hierarchy_value` per mutex is safe because at most one thread can hold the
 mutex. Try the same design for a `hierarchical_shared_mutex` and it breaks immediately:
 several readers hold it at once, each with a different previous level, and one slot
 cannot hold them all. That version needs a per-thread stack.

- **The cost is small but not zero, and turning it off defeats it.** Every lock adds a
 thread-local read and a branch. Tempting to compile the check out in release builds,
 but production is where the schedules you never tested happen, and a hierarchy that only
 exists in debug builds catches only the bugs you could already reproduce.

## 7. Interview follow-ups

**"Why does the hierarchy check fire even single-threaded, with no other thread in the
program at all? What is it actually checking?"** It's checking the *order this one thread
took locks in*, not any interaction between threads, a per-thread "how far down have I
already descended" counter, compared against the level of whatever lock is being attempted
next. That's exactly what turns a scheduling-dependent hang (needs the unlucky interleaving
to show up) into a deterministic assertion failure that fires the first time the offending
call path runs at all, on any machine, with any number of threads.

**"scoped_lock fixed 005's bank-transfer deadlock. Why can't it fix this one?"** Because
`scoped_lock` needs to see both mutexes *in the same expression* to do anything, it can
only avoid a cycle it's given both halves of. Here, `cache_lookup` only ever sees `cache_m`
directly and calls into code that separately locks `store_m` several stack frames down;
nothing in the source holds both names at once. The cycle exists purely in the call graph,
which `scoped_lock` has no visibility into at all.

**"You measured that removing the sleep still deadlocked 20/20 times in this demo, just
later. Does that mean this bug is actually easy to catch with enough testing?"** Only because
this demo hammers both call paths in a tight loop on ten real cores, an artificially wide
attack surface. Production code that exercises `cache_lookup` and `store_flush` rarely, and
not in a tight loop, narrows that same race window down to nanoseconds; the reading is
explicit that this is exactly how such bugs survive CI and then hang a service at 3am under
real, much lower-frequency traffic. "It reproduced in a stress demo" and "it will reproduce
in CI" are different claims.

**"Small machine, one core, does the hierarchy check still matter, or is this purely a
multi-core problem?"** Still matters, unchanged, a hierarchy violation is detected from the
*order* one thread took locks in, which exists identically whether that thread is one of ten
truly parallel threads or the only thread currently running on a single core between context
switches. The underlying deadlock itself still needs a second thread's opposite-order path
to actually interleave and hang, but the hierarchy's *detection* doesn't wait for that
interleaving to happen at all, which is the whole point.

**"You'd need to add a new subsystem in between cache and storage in the layering, how do
you pick its level without renumbering everything that already exists?"** This is exactly
why the reading uses widely spaced numbers (10000, 5000, 1000) rather than consecutive ones
, a new layer between cache (10000) and storage (5000) can take, say, 7500, without touching
either existing constant. Tightly packed levels (3, 2, 1) would force renumbering every
lower layer the moment a new one needs to be inserted between two adjacent existing ones.
