## 1. Reframe the problem

A `std::thread` object is not the thread. It is a **handle that owns** a thread of
execution, in the same way `unique_ptr` owns memory and `fstream` owns a file.

Every owning handle must answer one question: *what happens when I go out of scope?*
`unique_ptr` frees. `fstream` closes. `std::thread` **kills your process**.

That sounds absurd until you see the alternatives. The destructor has exactly two choices
and no way to pick correctly:

- **Join.** Block until the thread finishes. But the thread may never finish, so a
  destructor could hang forever. Destructors that hang are catastrophic. They run during
  exception unwinding.
- **Detach.** Let it run loose. But it probably references the very stack frame being
  destroyed, so you get silent memory corruption.

The committee decided that a guaranteed loud crash beats an unpredictable quiet disaster.
So: **you must state your intent explicitly, every time.** The exercise is to make that
statement automatic instead of manual.

## 2. The tools

```cpp
std::thread t(work);
t.joinable();     // true if it still owns a running thread
t.join();         // block until done; afterwards joinable() == false
t.detach();       // abandon it; afterwards joinable() == false
```

**Threads are move-only.** You cannot copy one. Two owners of one thread makes no sense.
But you can transfer ownership:

```cpp
std::thread a(work);
std::thread b = std::move(a);     // b owns it now; a is empty
std::vector<std::thread> pool;
pool.push_back(std::move(b));     // this is why containers of threads work
```

Moving is what lets you return a thread from a factory, or keep threads in a vector.

**RAII** is the fix. Wrap the handle in a type whose destructor makes the decision for you:

```cpp
class joining_thread {
    std::thread t;
public:
    ~joining_thread() { if (t.joinable()) t.join(); }
};
```

C++20 ships this as `std::jthread`. It also adds cooperative cancellation. Writing it
yourself once is worth doing, because the move-assignment operator has a trap in it.

## 3. The broken version, first

```cpp
void leaky() {
    std::thread t([]{ std::printf("worker ran\n"); });
}   // no join, no detach
int main() { std::printf("before\n"); leaky(); std::printf("after\n"); }
```

Actual output:

```
libc++abi: terminating
before
worker ran
exit=134
```

`after` never printed. Exit 134 is 128 + 6, killed by `SIGABRT`. The whole process died
because one function forgot one call.

Note this is **not** a rare timing bug. It happens every single run, deterministically.
That is deliberate: the standard made the mistake impossible to miss rather than making it
occasionally survivable.

Here's a subtler broken version, the manual fix that looks correct:

```cpp
void risky() {
    std::thread t(work);
    do_something();     // if this throws, we never reach join()
    t.join();
}
```

This is correct only while nothing between the launch and the join can throw. That is not
a property you can maintain. Someone adds a `return`, or a function that used to be
`noexcept` stops being so. Manual `join()` is a bug waiting for a maintainer.

## 4. Real-world usage

**Why `terminate` was chosen.** Boost.Thread originally *detached* in the destructor.
It was changed to terminate after years of reports where a detached thread outlived the
data it referenced and produced corruption that was impossible to trace back to its cause.
The reasoning: a program that crashes at the exact line of the mistake is cheaper to fix
than one that corrupts memory somewhere else, later, under load.

**Where you see this in production:**

- **Containers of threads.** `std::vector<std::jthread>` for a worker pool. Move support is
  what makes this possible at all. Without it you would need pointers.
- **Pipeline stages.** Each stage owns its thread as a member. The stage's destructor
  joins on cleanup, so shutdown order follows object destruction order, something you
  already understand. There's no separate shutdown protocol to get wrong.
- **Factories.** A function that configures and returns a running thread, as in the
  previous question.

**Where NOT to use it:**

- **Don't write `joining_thread` in new code.** Use `std::jthread` instead. It does this
  plus `stop_token` cancellation. Write your own only to understand it, or on pre-C++20.
- **Don't join in a destructor when the thread might not finish.** A joining destructor on
  a thread waiting for network input will hang your shutdown forever. Those threads need a
  cancellation signal before the join. That's exactly why `jthread` carries a `stop_token`
  and requests a stop before joining.
- **`detach()` is almost always wrong.** It is right only for a thread that owns everything
  it touches and whose completion nobody needs to observe. If you cannot state both, you
  want a joining handle.

## 5. Performance

Moving a `std::thread` is a pointer-sized swap. It's effectively free, with no
synchronization needed. Passing threads around by move costs nothing.

`join()` costs whatever the thread has left to do, plus a wakeup. It is not a busy-wait.
The joining thread sleeps.

The relevant number is thread creation, measured on this machine at **~26–36 µs**. That is
the argument for pools. If your tasks are shorter than ~2 ms, creating a thread per task
loses to reusing threads.

## 6. Where this solution fails

- **Move-assignment must join the thread it is discarding.** If `a = std::move(b)` simply
  overwrites `a`'s handle, `a`'s original thread is dropped without join or detach.
  `std::thread`'s assignment operator then calls `terminate()`. This is the trap in the
  exercise.
- **Self-move.** `a = std::move(a)` must not join and then use a dead handle. Guard it, or
  use the copy-and-swap shape which is naturally safe.
- **A joining destructor can hang.** No timeout exists. If the thread blocks forever, your
  destructor blocks forever. If that happens during exception unwinding, you get a process
  that cannot even report why it is stuck.
- **Exceptions thrown inside the thread do not propagate.** They call `terminate()` at the
  thread boundary. Catching them requires `std::promise`/`packaged_task` to carry the
  exception across. That's chapter 4.
- **Destruction order between threads and the data they use.** A member `jthread` declared
  *before* the data it touches is destroyed *after* it, so the join happens after the data
  is gone. Declare threads last in a class, so they are joined first.

## 7. Interview follow-ups

**"Why does the destructor call terminate() instead of just calling join() for you
automatically?"** Because a silent, automatic join can hang the calling thread forever with
no visible cause. If the thread being destroyed is stuck, waiting on I/O, deadlocked,
whatever, the destructor blocks too. Destructors run during exception unwinding, stack
cleanup, and program exit, places where an unexplained hang is far worse than a crash with
a clear line number. `terminate()` is the loud failure the committee chose over the silent
one. `std::jthread`'s destructor *does* join automatically, but only because it also
carries a `stop_token`. It requests a stop first, and that's what keeps the join from
hanging indefinitely on well-behaved code.

**"Move-assignment has a trap. Walk through exactly what goes wrong if you get it naively
wrong."** `a = std::move(b)` has to dispose of whatever thread `a` was already holding
before taking on `b`'s. A naive assignment that just overwrites `a`'s internal handle drops
that original thread with no join and no detach. `std::thread`'s own move-assignment
operator calls `std::terminate()` specifically to catch this, the same reasoning as the
destructor. The fix, if you're hand-rolling this, is to join or detach the old thread as
part of the assignment, before taking ownership of the new one.

**"Small machine. Could you just always create a new thread per task instead of a pool,
given move is basically free?"** Move being free doesn't offset thread *creation* being
expensive. That's measured at ~26-36µs on this machine, the same number this course's
earliest chapter-1 exercise measured. For tasks shorter than roughly 2ms, creating a thread
per task loses to reusing pooled threads regardless of how cheap moving the handle itself
is. That 2ms figure is the break-even this course measured. Move-cost and creation-cost are
two completely different numbers.

**"A joining thread's task throws an exception. What actually happens, and how would you
find out in production?"** The exception propagates up to the thread's entry function.
Finding no handler there, it calls `std::terminate()`. It does *not* propagate across the
thread boundary to whoever joins it. This is a hard crash, usually with a libc++abi message
on stderr identifying the uncaught exception type. That's often the only diagnostic you
get, unless the crash reporting infrastructure captures the abort signal's context.
Carrying the exception across the boundary on purpose needs `std::promise`/`packaged_task`,
covered in [[007-async-future]] and [[048-exceptions-across-threads]].

**"You have a class with a jthread member and some data members it operates on. Does field
declaration order actually matter here, and how would you verify it?"** Yes. C++ destroys
members in reverse declaration order, so a thread member declared *before* the data it
touches gets destroyed, and joined, after that data is already gone. The join still
happens, but if the thread is still running when the destructor starts, its last actions
may already be touching freed memory. Declaring the thread member *last* means it's
destroyed, and joined, first, before any data it depends on goes away. This is exactly the
kind of ordering bug that won't show up in a quick manual test. Catching it needs either a
TSan or ASan run under real destruction timing, or a deliberate review checklist for every
class that mixes a `jthread` member with other state.
