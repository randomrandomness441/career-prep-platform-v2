`std::shared_mutex` supports two lock modes: `std::shared_lock` (many holders at once — the
"read" mode) and `std::unique_lock` (exactly one holder, excluding everyone else — the
"write" mode). Which mode should `get()` use? Which should `set()` use, and what goes wrong
if you pick the same mode for both "to keep it simple"?

---

Run the boilerplate against the tests before assuming it's fine — it either hangs or
ThreadSanitizer flags a data race, depending on the run. Both `get()` and `set()` there use
`std::shared_lock`. That means multiple `set()` calls can run *at the same time*, each
believing it has safe access, mutating the same `std::unordered_map` with no exclusion
between them at all.

---

`set()` mutates shared state — it needs exclusivity against every other `get()` and `set()`,
the same as any critical section protected by a plain mutex. `std::unique_lock<std::shared_mutex>`
gives you that, while still letting `get()` calls run concurrently with each other (just not
concurrently with a `set()`). The rule: readers take `shared_lock`, writers take
`unique_lock` — never the other way around, and never the same mode for both.
