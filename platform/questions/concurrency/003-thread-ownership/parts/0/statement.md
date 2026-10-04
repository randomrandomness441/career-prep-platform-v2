# Transferring Thread Ownership

## ELI5: the campfire you must never walk away from

You lit a campfire. Camping rule number one: you never leave a lit campfire unattended.
You put it out before you walk away, no matter what.

A plain `std::thread` is like a campfire with no such rule built in. Walking away means
the thread object gets destroyed. If it's still burning, still joinable, still running or
waiting to be joined, the whole campsite catches fire: the process calls `std::terminate`
and dies. You have to remember to douse it every single time, by hand.

`joining_thread` is a campfire that puts itself out automatically the moment you walk
away. You cannot forget.

## What you're actually building

```cpp
class joining_thread {
public:
    joining_thread() noexcept = default;

    explicit joining_thread(std::function<void()> f);

    joining_thread(joining_thread&&) noexcept;
    joining_thread& operator=(joining_thread&&) noexcept;

    joining_thread(const joining_thread&) = delete;
    joining_thread& operator=(const joining_thread&) = delete;

    ~joining_thread();

    bool joinable() const noexcept;
    void join();
    std::thread::id get_id() const noexcept;
};
```

## Requirements

1. The destructor joins the thread, douses the fire, if there's still one to join.
2. Movable. You can hand the "watching this fire" duty to someone else. That's what lets
   it live in a `std::vector` and be returned from functions.
3. **Not copyable.** Two people simultaneously "in charge" of putting out the same fire
   is meaningless. Decide on one owner.
4. Move-assignment must not silently drop the fire it's replacing.

Requirement 4 is the one that bites. If you're already tending campfire A and someone
hands you campfire B to watch instead, you can't just walk off toward B and leave A
burning. You have to put A out first. Work out what `std::thread::operator=` actually
does when the target already owns a running thread, before you write your version.

## Why the constraints exist

- **Build on `std::thread`, not `std::jthread`.** `std::jthread` already does this for
  you. The whole point of this exercise is to build the mechanism yourself and
  understand why it exists.
- **`a = std::move(a)` must not blow up.** That's watching your own fire, handed to
  yourself. It's an easy edge case to get wrong once you're juggling "put out the old
  one first."
