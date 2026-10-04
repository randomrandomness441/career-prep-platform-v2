# Read-Write Locks (std::shared_mutex)

## ELI5: a reference shelf that many people can read at once

Picture a library's reference shelf. Dozens of people can stand there and read the same
book at the same time, reading never bothers anyone else who's also reading. But when
the librarian needs to swap a book out for a new edition, *everyone* has to step back
until that swap is finished, you can't have someone mid-sentence in a book that's being
pulled off the shelf right now. Reading is cheap and shareable; updating needs the whole
shelf to itself, briefly.

Locking every reader out just because *someone else* is also only reading, the way a
plain single-occupancy lock would, throws away that entire advantage for no reason.

## What you're actually building

A config store, read far more often than it's written:

```cpp
class ConfigStore {
public:
    void set(std::string key, std::string value);
    std::string get(const std::string& key) const; // "" if missing
};
```

Many threads call `get()` concurrently; other threads call `set()` concurrently with all
of that. Use `std::shared_mutex` so reads don't need to serialize against each other.

## Requirements

1. Multiple `get()` calls may run truly concurrently with each other, many readers, one
 shelf, no waiting on each other.
2. A `set()` call must have exclusive access, no `get()` and no other `set()` may be
 running at the same moment as any given `set()` call, the librarian's swap gets the
 shelf to itself.
3. Every key ever `set()` must later be `get()`-able with its correct value, under real
 concurrent load from many writer and reader threads at once.

## Why the constraints exist

**Use `std::shared_mutex`, `std::shared_lock`, and `std::unique_lock`**, not a plain
`std::mutex` for everything. A plain mutex would satisfy correctness trivially, but it
defeats the entire point: readers would serialize against each other for no reason, the
same as locking the whole reading room just because two people both want to *read*.
