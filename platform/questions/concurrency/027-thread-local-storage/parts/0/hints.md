Watch what the broken version actually returns under load: each thread gets a slice of
ONE sequence — 1, 2, 3 ... 800000 shared among eight threads, with gaps where other
threads' tickets fell. To pass, each thread needs its own counter — under the same
name, reachable from any function on that thread. What storage duration gives every
thread its own private object with one declaration?
---
`thread_local`: one instance per thread. Initialized the first time the owning thread
reaches the declaration, destroyed when that thread exits. Pair it with `static`
inside the class —

```cpp
inline static thread_local long tickets_ = 0;
```

`static` gives the variable one name and process-lifetime; `thread_local` gives each
thread its own object under that name. The dispenser stays shared and atomic — a
station id must be unique across the whole process, so that state is genuinely
per-process.
---
The lazy station assignment `if (station_ < 0) station_ = next_station_.fetch_add(...)`
is safe with no lock precisely because `station_` is `thread_local`: each thread runs
the check once, against storage only it can touch. In the shared version, two threads
both read -1, both fetch_add, and the last write wins — duplicate station ids. Same
line, opposite behaviour, decided entirely by storage duration.
