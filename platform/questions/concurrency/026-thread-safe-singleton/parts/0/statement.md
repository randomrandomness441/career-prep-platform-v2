# Thread-Safe Singleton Initialization

## ELI5: a clubhouse that builds itself the first time anyone visits

Imagine a clubhouse that doesn't exist yet, it gets built automatically the very first
time *anyone* walks up to visit. After that, it's just there, permanently, and everyone
who visits afterward walks into the same, already-finished clubhouse.

The tricky part: what if two people walk up at the *exact same moment*, both finding no
clubhouse there yet? If both of them start building, you get two clubhouses (or one
half-built mess where two sets of hands were nailing boards at once). What you actually
want: exactly one construction happens, no matter how many people arrive at once for the
first visit, and everyone else just waits for that one build to finish and then walks
into the same building.

## What you're actually building

`singleton<T>::get()`, the one door to a lazily created, process-wide instance of `T`.
Any thread may call it, at any time, with no external synchronization.

```cpp
template <typename T>
class singleton {
public:
    static T& get(); // returns the single instance of T, constructing it on first use
};
```

## Requirements

1. Any number of threads may call `get()` concurrently. Callers lock nothing themselves
 , they just walk up to the door.
2. `T`'s constructor runs **exactly once**, no matter how many threads race on the first
 call, exactly one construction crew, ever.
3. Every thread that gets a reference sees a **fully constructed** object, a thread that
 observes "the clubhouse is built" must also observe every wall and window the builder
 put up, not a half-finished structure that merely looks occupied from outside.
4. If `T`'s constructor throws, the build collapses mid-way, the failure doesn't stick:
 the next call to `get()` attempts construction again, from scratch.

## Why the constraints exist

- **`if (inst_ == nullptr) inst_ = new T(...)` is the bug you're replacing, not a
 starting point.** Its check and its act are two separate steps, and other threads can
 run in between them, the exact "two roommates, one slice of pizza" gap from
 [[004-interface-races]], applied to construction instead of a stack.
- **Once initialization has completed, `get()` must cost no more than a load or two**,
 no taking a mutex on every single call for the rest of the program's life. Checking "is
 the clubhouse built" should be nearly free once it obviously is.
- **Initialization being thread-safe is the only guarantee here.** Making the
 singleton's own *member functions* thread-safe afterward is a separate problem, and
 not part of this one.
