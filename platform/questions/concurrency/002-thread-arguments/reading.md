## 1. Reframe the problem

`std::thread` **copies every argument you hand it** into storage the new thread owns.
That is the single fact behind every bug in this question.

It is not an arbitrary choice. The new thread may outlive the function that started it,
so anything it touches must survive independently. Copying is the only safe default.
The library cannot know how long your local variables will live.

So the real question is never "how do I pass this?" It is: **who owns this data, and is
it guaranteed to still be alive when the thread touches it?** Every fix below is a
different answer to that.

## 2. The tools

### Passing by value (the default)

```cpp
void worker(int n);
std::thread t(worker, 42); // 42 is copied into the thread's storage
```

The copy happens **when the thread is constructed**, on the calling thread, not later.

### Passing a reference: `std::ref` / `std::cref`

Because arguments are copied, this does not compile:

```cpp
void worker(Counter& c);
std::thread t(worker, c); // error, and a famously unreadable one
```

You must say explicitly that you meant a reference:

```cpp
std::thread t(worker, std::ref(c)); // mutable reference
std::thread t(worker, std::cref(c)); // const reference
```

`std::ref` wraps a pointer in a copyable object, so the copy still happens. It copies the
*wrapper*, and the wrapper still points at your original.

### Lambdas: the capture list is the same decision

```cpp
std::thread t([c, n]() mutable { ... }); // copies c
std::thread t([&c, n] { ... }); // refers to the caller's c
```

`[=]` and `[&]` capture everything by copy or by reference. Prefer naming what you capture.
With `[&]` it is very easy to accidentally capture `this`, or a local you did not think
about, and never notice.

### Move-only arguments

`std::unique_ptr` cannot be copied, so it must be moved in:

```cpp
auto p = std::make_unique<Job>();
std::thread t(run, std::move(p)); // ownership transfers into the thread
// p is null here
```

## 3. The broken versions, first

### Broken 1, the update silently disappears

```cpp
void launch(Counter& c, int n) {
    std::thread t([c, n]() mutable { for (int i=0;i<n;++i) c.value++; });
    t.join();
}
```

Compiles cleanly, no warnings. Actual output:

```
captured [c, n] (copy) -> caller sees 0
captured [&c, n] (ref) -> caller sees 1000
```

The thread incremented a copy 1000 times, then the copy was destroyed. **This is the
dangerous one**, because there is no crash, no sanitizer report, and nothing to grep for.
The only symptom is a number that stays zero. `mutable` is the tell: you needed it only
because you were mutating a copy.

### Broken 2, the error message you will actually meet

```cpp
std::thread t(worker, c, 1000); // worker takes Counter&
```

```
error: no matching function for call to '__invoke'
note: candidate template ignored: substitution failure [with _Args =
 <__libcpp_remove_reference_t<void (*&)(Counter &, int)>, ...>]:
 no type named 'type' in 'std::__invoke_result_impl<void, void (*)(Counter&, int), Counter, int>'
```

Nothing in that text says "you forgot `std::ref`". Recognising this error by shape is worth
more than understanding it: **`__invoke` substitution failure from a `std::thread` line
means an argument mismatch, and usually a missing `std::ref`.**

### Broken 3, the one that looks like it works

```cpp
void oops() {
    int local = 42;
    std::thread t([&local] {
        std::this_thread::sleep_for(50ms);
        std::printf("%d\n", local); // local is long dead
    });
    t.detach();
} // local dies here
```

Run it:

```
detached thread sees local = 42
```

**Correct output. From memory that was destroyed 50ms earlier.** The stack frame was still
mapped and nothing had overwritten it yet. Under load, with a deeper call stack, it prints
garbage or crashes.

Even AddressSanitizer misses this by default. It has to be asked:

```
$ clang++ -fsanitize=address -fsanitize-address-use-after-return=always ...
$ ASAN_OPTIONS=detect_stack_use_after_return=1 ./a.out

ERROR: AddressSanitizer: stack-use-after-return on address 0x000105245060
READ of size 4 at 0x000105245060 thread T1
```

Worth remembering when you are hunting a heisenbug: **stack-use-after-return is off by
default.**

## 4. Real-world usage

**Why copying was chosen.** In pthreads you pass `void*` and the lifetime problem is
entirely yours. The classic C bug is passing `&i` from a loop and having every thread read
the same, now-modified, variable. When `std::thread` was designed for C++11 the committee
made copy the default precisely to kill that bug class. The unsafe thing became the one
you have to type extra characters for. `std::ref` is deliberately ugly: it is a place to
pause and ask whether the referent outlives the thread.

**Where you see this in production:**

- **Worker pools.** Tasks carry copies or `shared_ptr`s, never raw references to caller
 stack frames, because a queued task runs long after its submitter returned.
- **Async callbacks.** Anything registered with a callback and then forgotten, a timer, an
 I/O completion, a subscription, must own its data. This is why `shared_from_this()`
 exists, and why capturing `this` in a lambda that outlives the object is one of the most
 common crashes in C++ services.
- **`std::ref` is legitimate for joined threads.** If you `join()` before the referent dies,
 a reference is correct and free. That is the normal case for parallel algorithms splitting
 one array across threads.

**Where NOT to use `std::ref`:**

- **Never with `detach()`.** You have declared "this thread outlives my scope" and
 simultaneously "this thread points into my scope". Those cannot both be safe.
- **Never for anything owned by a shorter-lived object.** A reference to a member is a
 reference to the enclosing object's lifetime, not just that field.
- **When ownership should transfer, use `std::move`,** not a reference. Moving makes the
 handoff explicit and the source unusable, so the compiler helps you.

## 5. Performance

Copying is safe but not free. Launching a thread with a 10 MB `vector`, 40 launches:

| Call | µs per launch |
|---|---|
| `std::thread(f, big)`, decay-copy | **548.9** |
| `std::thread(f, std::cref(big))` | **26.8** |
| cost of the copy alone | **522.1** |

The copy is **20x the entire cost of launching a thread**. For reference, thread creation
itself was measured at ~26–36 µs on this machine.

So: pass by value for small things, and it costs nothing worth measuring. For large data
you have three options: `std::ref`/`cref` when you will join before it dies, `std::move`
when ownership should transfer, or `shared_ptr` when lifetime genuinely is shared. Choosing
`cref` here is not a micro-optimisation. It is a 20x difference on the launch path.

## 6. Where this solution fails

- **`std::ref` plus an early return.** Your function joins at the bottom, but someone adds
 a `return` in the middle. The thread is now detached-by-destructor. Except it is not:
 `std::thread`'s destructor calls `std::terminate()`. You get a crash instead of a dangling
 reference, which is the better of the two failures but still a crash. `std::jthread` joins
 instead.
- **`std::ref` plus an exception.** Same shape: the referent's scope unwinds while the
 thread runs. Use `jthread` or a guard whose destructor joins.
- **Capturing `this` implicitly.** `[&]` captures `this`, so a lambda referring to any
 member silently depends on the object outliving the thread. In C++20 write `[this]` or
 `[self = shared_from_this()]` so the dependency is visible.
- **Copies of types with shared state.** Copying a `shared_ptr` into a thread is safe for
 the pointer but says nothing about the pointee. Two threads now share the object it
 points to, and that object still needs its own synchronisation.
- **Reference to a `vector` element.** `std::ref(v[0])` is valid until the vector
 reallocates, after which it points at freed memory. Reference the container, not an
 element, unless you can prove no reallocation happens.

## 7. Interview follow-ups

**"Broken 1's `c.value` silently stayed at 0, no crash, no sanitizer warning. How would you
actually find this bug in a large codebase, given nothing screams at you?"** Code review for
the specific shape, a lambda captures a value by copy, then mutates the copy, and the caller
later reads the original expecting the mutation, is the realistic first line of defense,
since neither TSan nor ASan flags it by default. There's no race (the copy is genuinely
private to the thread) and no invalid memory access. A targeted test that checks the
*caller's* value after `join()`, not just that the program didn't crash, is the dynamic
check. This exercise's own test does exactly that.

**"Broken 3 printed the correct value from memory that was already destroyed. Why did that
happen to work, and why is trusting it dangerous?"** The stack frame's memory was still
mapped and nothing had overwritten it in the 50ms before the detached thread read it. That's
an accident of this particular run's timing and call depth, not a guarantee. A deeper call
stack, a different function called in between, or a different optimization level could
overwrite that memory before the detached thread gets to it, turning "looks correct" into
garbage or a crash with no code change at all. This is why `stack-use-after-return` detection
is off by default in ASan (it's expensive) and has to be explicitly requested. A program can
pass every default sanitizer run and still have this exact bug.

**"Small machine, thread creation is expensive relative to the work. Does the copy-vs-cref
choice still matter at 528µs vs 27µs, if you're only launching a handful of threads total?"**
The absolute numbers stay the same regardless of core count, copying is CPU work, not
something more cores help with directly. But whether the difference *matters* depends on how
many launches happen and how big the argument is. For one-off thread creation of small
arguments, neither number is worth worrying about. The measurement here specifically used a
10MB vector to make the cost visible, not because every thread argument is that large.

**"You're passing a shared_ptr into 1000 worker threads at once, what's actually happening
per launch, and does it scale?"** Each `std::thread` construction copies the `shared_ptr`
itself, a pointer plus an atomic refcount increment. That's cheap and thread-safe by design,
not the underlying object. At 1000 concurrent launches, that's 1000 atomic increments on one
shared control-block refcount, which is real (if usually small) contention. See
[[030-atomics-vs-mutexes]] for how atomic contention actually scales under load rather than
assuming it's free just because it's "just an atomic op."

**"A worker thread throws partway through using a std::ref'd object, and the launching
function has already moved on, what's the failure mode in production, and how would you
catch it before it ships?"** Exactly [[006-hierarchical-mutex]]'s and this reading's own
"ref plus an exception" case: the referent's scope unwinds while the thread still holds a
reference to it, and the thread's next access is a use-after-free with no crash guaranteed at
the moment of the bug. It may corrupt memory silently instead. Catching it before shipping
means either running under ASan with stack-use-after-return detection enabled specifically
(not the default), or, more reliably, switching to `std::jthread` or an RAII join-guard so
the launching scope can't exit while the thread still references it, making the bug
structurally impossible rather than something you have to catch by testing.
