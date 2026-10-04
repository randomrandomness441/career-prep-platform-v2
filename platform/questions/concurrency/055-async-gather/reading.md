## 1. Reframe the problem

"Gather the results of several async tasks" sounds like it should mean "collect them as
they arrive", that's how a lot of async APIs in other languages actually work (JavaScript's
`Promise.race`, Python's `asyncio.as_completed`), so the instinct isn't wrong in general,
it's wrong for *this* contract specifically. The requirement here is closer to
`Promise.all` or `asyncio.gather`: the caller already knows which task is which by its
position, and wants that position preserved in the output, independent of whichever task
happens to finish first. The reframe: this isn't a "wait for the next thing to happen"
problem, it's a "wait for N specific, already-identified things, in a specific order"
problem, and a `std::future` already knows how to do exactly that for one task; the
question is what to do with N of them.

## 3. The broken version, first

The boilerplate polls every future round-robin and appends whichever is ready first:

```cpp
if (futures[i].wait_for(std::chrono::microseconds(0)) == std::future_status::ready) {
    results.push_back(futures[i].get());
    ...
}
```

**Why it looks right:** it's a completely reasonable, working way to collect N asynchronous
results, nothing here is incorrect as *concurrency* code; every future does eventually get
waited on and consumed exactly once. It answers a different question than the one asked:
"what order did they finish in," not "what order were they given in."

Running it, 20 tasks, each doing one atomic operation and then sleeping for a duration that
*decreases* with index (so task 19 finishes first, task 0 finishes last):

```
results[0] = 19, expected 0 -- gather() returned results in completion
order, not input order
```

Every run: the polling loop finds whichever future's task happens to be done first, which is
task 19's, the exact opposite of what `results[0]` is supposed to hold.

The fix removes the polling entirely:

```cpp
for (auto& f : futures) results.push_back(f.get());
```

Same reverse-completion-order test: exact input order, every run, because `futures[0].get()`
simply blocks until task 0 finishes, however long that takes, before the loop even looks at
`futures[1]`.

## 6. Where this solution fails

- **A slow task at the front blocks reporting every faster task behind it**, even though
 their results are already sitting there, ready. This exercise's contract explicitly wants
 input order, so that's not a bug relative to *this* requirement, but it's the direct
 trade-off against the polling version's behavior, and it's worth being able to say out
 loud which one a given caller actually needs.
- **Only the first exception (by input position) is ever seen**, once `futures[0].get()`
 throws, the loop unwinds immediately; `futures[1]`'s task may also have thrown, or may
 still be running, and this design never finds out. A caller that needs to know about
 *every* failure, not just the first one encountered in order, needs to catch each
 `.get()` individually and collect exceptions rather than let the first one propagate and
 stop the loop.
- **`std::future::get()` is one-shot**, this function consumes every future it's given
 (they're taken by value, moved from), which is fine for `gather`'s own contract but means
 the caller can't hold onto any of these futures afterward to inspect them again.
- **This says nothing about tasks that never finish.** If one task hangs forever, `gather`
 hangs forever waiting on its `.get()`, with no timeout and no way to report partial
 results for the tasks that *did* finish. See [[041-deadlock-detection-watchdogs]] for the
 general pattern of adding a deadline to an otherwise-unbounded wait.

## 7. Interview follow-ups

**"How would you build the completion-order version properly, given it's a real, useful
API shape too (just not what this exercise asked for)?"** `std::future` alone doesn't
efficiently support "tell me which one becomes ready first" without polling, the standard
library has no `wait_any`. A real implementation typically has each task, on completion,
push its own (index, result) onto a thread-safe queue itself (rather than the collector
polling futures from outside), and the collector just pops from that queue N times, pushing
the "notify on completion" responsibility into the task itself instead of polling externally.

**"What if you wanted BOTH, process results as they arrive, but still know which original
task each one came from?"** Don't discard the index: instead of `std::future<T>`, use
`std::future<std::pair<int, T>>` (or have each task's lambda capture and return its own
index alongside its result), then a completion-order collector (the "wrong" version for
this exercise) becomes exactly right for that different, index-aware contract.

**"You mentioned this only surfaces the first exception, how would you collect ALL of
them?"** Catch around each individual `.get()` inside the loop rather than letting one throw
propagate the whole function, storing either the result or the caught exception per index
(`std::variant<T, std::exception_ptr>`, or a parallel vector of `std::exception_ptr`), then
the caller can inspect exactly which of the N tasks failed and why, instead of only ever
learning about whichever one the loop happened to reach first.
