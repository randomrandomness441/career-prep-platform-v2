## 1. Reframe the problem

A `try`/`catch` only sees exceptions thrown on its own thread, in the frames still on that
thread's stack when the `throw` happens. A `std::thread`'s entry function runs on a
different stack, with nothing above it on that thread, there is no caller frame for the
exception to unwind into if nobody catches it there. C++ doesn't leave that case undefined:
it has an explicit rule. **If an exception is still propagating when a thread's entry
function returns, `std::terminate()` runs**, by default that calls `abort()`, and the
whole process dies, immediately, no matter what any other thread was doing.

So "let the exception propagate", the thing you'd do without thinking twice on a single
thread, is the one thing you must never do on a thread's entry function. The exception has
to be *caught right there* and carried somewhere a different thread can pick it back up.
That "somewhere" is a `std::promise`: it has two ways to fill in the eventual answer,
`set_value(...)` for success and `set_exception(std::exception_ptr)` for failure, and the
`std::future` on the other end surfaces whichever one happened by either returning a value
from `get()` or having `get()` rethrow.

`std::async` and `std::packaged_task` already wrap this pattern for you, every task they run
is invoked inside a `try`/`catch` that funnels a thrown exception into `set_exception`
automatically. This exercise is building that wrapper by hand, on top of a raw
`std::thread`, so the mechanism isn't a black box.

## 3. The broken version, first

```cpp
std::thread([p, f = std::move(f)]() mutable {
    p->set_value(f()); // <-- the bug: nothing catches an exception from f()
}).detach();
```

**Why it looks right, and who it fools:** it's the direct translation of "call `f`, put the
result in the promise", one line, does what it says, and every *successful* call behaves
exactly like the fixed version. Nothing about reading this code signals danger, because
nothing about it is wrong when `f` doesn't throw. It's also easy to reach for because it's
almost the same shape as returning a value from an ordinary function, the missing piece
(that a thread boundary eats exceptions where a function return doesn't) isn't visible
anywhere in the syntax.

Isolated, minimal reproduction, a bare `std::thread` whose entry function throws, run for
real:

```cpp
std::thread t([] { throw std::runtime_error("boom"); });
t.join();
```

Actual output:

```
before
worker started, about to throw
libc++abi: terminating due to uncaught exception of type std::runtime_error: boom
```
exit code 134 (128 + `SIGABRT`). `after` is never printed, the process is gone before
`join()` could return control to `main`.

This is **not a timing-dependent bug**. It fires on every single run, deterministically,
the standard didn't leave room for "maybe it propagates, maybe it doesn't." That determinism
is what makes it testable without any of this course's usual race-hunting machinery
(TSan, `SHAKE()`): running `run_task` with a throwing task against the naive version kills
the process every time, verified here by forking before the call so only the child dies:

```
run_task let the task's exception escape the worker thread: the process was killed by
signal 6 (Abort trap: 6), consistent with std::terminate(). Catch the exception on the
worker thread and hand it to the promise with set_exception(std::current_exception())
instead of letting it propagate out of the thread's entry function.
```

The fix is one `try`/`catch` around the body of the thread's lambda:

```cpp
try {
    p->set_value(f());
} catch (...) {
    p->set_exception(std::current_exception());
}
```

`std::current_exception()` is doing the actual work here: called inside a `catch` block, it
captures whichever exception is currently being handled into a `std::exception_ptr`, a
handle that keeps the exception object alive independent of the `catch` block that created
it. That's what makes it safe to store in the promise and let a *different thread*, possibly
long after this one has exited, call `std::rethrow_exception` on it (which is exactly what
`future::get()` does internally when the shared state holds an exception instead of a
value). Same throwing task, fixed version:

```
exceptions come back through future::get() instead of terminating, non-throwing and void
tasks work, and 20 concurrent tasks don't cross-talk
```

**Why this is the expected answer at senior level.** `catch (...)` looks like it's
swallowing information, real production code is usually told to catch specific types. Here
it's correct precisely because this code doesn't know what it's catching. `run_task` is
generic over any callable `F`; it cannot know in advance what exception types `f` might
throw, and its job isn't to *handle* the exception at all, only to *transport* it,
unchanged, to whoever called `get()`. `catch (...)` plus `current_exception()` is the one
tool in the language built for exactly that: catch anything, preserve everything, decide
nothing.

**The third mechanism, briefly:** `promise::set_exception` is the manual, general-purpose
version of what `packaged_task` and `std::async` do for you automatically. A
`std::packaged_task<R()>` wraps a callable and, when invoked, runs it inside its own
internal `try`/`catch`, success calls the promise's `set_value` for you, failure calls
`set_exception` for you, and `std::thread(std::move(task)).detach()` is all you write. This
question builds the `promise`/`set_exception` version by hand because that's the mechanism
underneath; the earlier `std::async`-based exercise in this course
([[007-async-future]]) shows the same guarantee, "an exception from the task comes back out
of `get()` as itself", delivered by the higher-level tool instead.

## 6. Where this solution fails

- **`std::async` with `std::launch::deferred` changes *when* this all happens, not
 whether it happens.** A deferred task doesn't run on a separate thread at all, it runs
 synchronously, inside `get()`, on the calling thread, the first time `get()` is called.
 The exception still comes out of `get()`, but now via an entirely ordinary same-thread
 `throw`/`catch`, and any "which thread did the work" assumption elsewhere in the program is
 wrong for a deferred task.
- **A future you never call `get()` on hides the exception forever.** If nobody ever calls
 `get()`, an exception captured in the shared state is simply discarded when the future
 is destroyed. No `terminate()`, no crash, no log line, the task failed and nothing
 noticed. (`std::async`'s *returned* future is a partial exception to this: letting the
 *last* future to an async-launched task's shared state destruct without a `get()` first
 makes that destructor block, waiting for the task, but it still doesn't rethrow. Silence,
 either way.)
- **Only the shared state remembers the exception was thrown *once*.** Calling `get()` a
 second time on the same future is itself an error (`std::future_error`,
 `future_already_retrieved` on the *second* `get()` call across two futures for the same
 state, or simply invalid on an already-retrieved single-use future), the exception (or
 value) is consumed, not cached for repeated reads. `shared_future` exists specifically to
 let multiple readers each get their own look at the same outcome.
- **`std::exception_ptr` keeps the exception object alive, including whatever it holds.**
 An exception carrying a large buffer or a reference into now-destroyed thread-local state
 is kept alive exactly as long as the `exception_ptr` is, usually fine, but worth knowing
 when the thrown type is unusually heavy.
- **A `catch (...)` that captures via `current_exception()` and later rethrows is not the
 same as never having caught it at all.** The rethrow at `get()` happens on the *calling*
 thread, stack traces, thread-local state visible via `catch`, and anything that depends
 on catching at the exact point and thread of the original `throw` will look different from
 a same-thread exception. Debugging a rethrown-from-another-thread exception needs this
 understood up front, or the original throw site looks like it vanished.
- **This wrapper detaches its worker thread.** Nothing about `run_task`'s interface lets the
 caller cancel a task in flight or find out it's still running versus abandoned, the
 future resolves when the task finishes or never, with no way to ask "how much longer."
 A production task-runner usually needs a cancellation token or a timeout on `get()`
 (`future::wait_for`) layered on top of this.

## 7. Interview follow-ups

**Q: What happens if an exception escapes a raw `std::thread`'s entry function?**
A: `std::terminate()` runs, which by default calls `abort()`, the entire process dies
immediately, on every run, deterministically. It doesn't matter whether other threads are
mid-operation, holding locks, or about to write something to disk; nothing gets a chance to
clean up.

**Q: How is `std::async`/`std::packaged_task` different?**
A: They invoke the wrapped callable inside their own `try`/`catch`. A thrown exception is
captured into the task's shared state via the promise's `set_exception`, and
`future::get()` rethrows it on whatever thread calls `get()`, the exception crosses the
thread boundary intact instead of killing the process.

**Q: What does `promise::set_exception` actually take, and where does the exception object
come from?**
A: A `std::exception_ptr`, an opaque, type-erased, reference-counted handle to an exception
object, obtained from `std::current_exception()` inside a `catch` block (or manufactured
directly with `std::make_exception_ptr`). It's the vehicle that lets the *type* and
*payload* of an arbitrary exception cross a thread boundary through ordinary data, the
`shared_state` inside a promise/future pair, rather than through the call stack, which is
the only path a normal `throw` can travel.

**Q: A task never gets its future's `get()` called. What happens to a captured exception?**
A: Nothing observable. It's discarded silently when the shared state is destroyed along with
the future, no crash, no log, no signal that the task failed at all. This is a common
"errors going nowhere" bug in fire-and-forget task systems: capturing the exception
correctly is necessary but not sufficient; something also has to actually call `get()` (or
otherwise inspect the future) for the failure to be seen.

**Q: How would you build a task-runner that logs every failed task automatically, without
trusting every caller to remember to call `get()` and catch?**
A: Wrap the promise-filling code so that, in the `catch` block, before calling
`set_exception`, it also logs the exception (message, task id) right there on the worker
thread, that's the one place guaranteed to run for every failure regardless of what the
caller ever does with the future. Relying on the caller's `get()`/`catch` for logging means
a caller who drops the future silently drops the failure too.

**Q: Failure injection, the task itself is fine, but the thread can't be created at all
(`std::thread`'s constructor throws `std::system_error`, e.g. the OS is out of thread
handles). Does this design handle that?**
A: Not as written, that `throw` happens in `run_task` itself, on the *caller's* thread,
before the `std::thread` object (and its lambda) exist at all. It propagates out of
`run_task` normally, like any other exception thrown by a normal function, there's no
thread boundary to cross yet, so no `promise`/`future` machinery is involved or needed. A
caller of `run_task` needs to be ready to catch it around the *call*, separately from
whatever it does with the returned future.
