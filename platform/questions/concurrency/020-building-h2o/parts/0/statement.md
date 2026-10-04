# Building H2O

## ELI5: a water-molecule assembly line

Hydrogen atoms and oxygen atoms keep showing up at a little assembly station, wanting to
bond into water molecules. Water is H₂O: two hydrogens and one oxygen, always. Atoms show
up in whatever order they please, sometimes a whole crowd of hydrogens arrives before a
single oxygen does, and your job is to let them bond correctly anyway: never let a third
hydrogen sneak into a molecule that already has two, never let a second oxygen sneak in
before the first molecule is complete.

## What you're actually building

Two kinds of thread arrive at the assembler. Hydrogen threads call `hydrogen()`, oxygen
threads call `oxygen()`. Each is handed a callback that emits its atom.

```cpp
class H2O {
public:
    H2O();
    void hydrogen(std::function<void()> releaseHydrogen);
    void oxygen(std::function<void()> releaseOxygen);
};
```

The emitted atoms form one long sequence, every call to a release callback appends one
character to it. Your job is to make sure that sequence reads as a stream of water
molecules: **split the output into consecutive groups of three. Every group must contain
exactly two `H` and one `O`.**

Order *within* a group doesn't matter, `HHO`, `HOH` and `OHH` are all a valid molecule.
What's forbidden is a third hydrogen showing up before the oxygen that completes the
current group, or a second oxygen showing up before the group is finished.

## Requirements

1. For a run with `2n` hydrogen threads and `n` oxygen threads, all `3n` calls must
 return. Nothing may be left blocked, no atom stuck waiting forever for a bonding
 partner that never comes.
2. Every consecutive group of three released atoms is exactly two `H` and one `O`.
3. It must work no matter what order the threads arrive in, including all `2n` hydrogen
 threads arriving before the first oxygen thread, which is what the tests do.
4. Each thread calls its method exactly once. Both methods may be called from any number
 of threads at the same time.

## Why the constraints exist

- **Block, don't poll.** No `sleep`-and-retry loops, an atom that can't bond yet
 actually sleeps.
- **Assume the caller supplies exactly `2n` hydrogens and `n` oxygens.** You don't have
 to cope with a supply that can never form a molecule.

The trap isn't "how do I count to three." It's deciding *where* the release callback is
allowed to happen relative to the counter update. Get that wrong and your counters will
be perfect while your actual output isn't.
