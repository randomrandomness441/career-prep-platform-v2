# Parallel Quicksort with std::async

## ELI5: sorting a huge pile of index cards

You've got a giant pile of index cards to sort. Quicksort's trick: pick one card as a
"pivot," split the rest into "smaller than the pivot" and "bigger than the pivot," and
then sort each pile the same way, recursively, until every pile is down to one card.

Doing this alone takes a while. So instead, every time you split the pile in two, you
hire a helper to sort one half while you sort the other half yourself, at the same time.
Two piles being sorted simultaneously is twice the speed, as long as you don't take the
"hire a helper" idea too literally and end up hiring a brand-new helper for every single
card. A pile of 300,000 cards does not need 300,000 helpers; it needs a *few* helpers,
each handling a healthy chunk, splitting further only while it's still worth the trouble.

## What you're actually building

```cpp
template <typename T>
std::vector<T> parallel_quick_sort(std::vector<T> input);
```

It must produce the same result as `std::sort` on the same elements, and it must
actually use more than one thread to get there, a correct sequential quicksort (doing
it all yourself, no helpers) is not an acceptable answer.

## Requirements

1. **Correct on every shape**, not just the average case: empty, one card, already
 sorted, reverse sorted, all-equal, few distinct values, negatives, duplicates.
2. **The caller's pile is untouched.** `input` is taken by value, sort a copy, don't
 mutate through a reference.
3. **It must genuinely use more than one thread.** The tests can tell, comparisons are
 instrumented to record which thread performs them. Sorting a large input entirely on
 the thread that called `parallel_quick_sort` (never actually hiring a helper) is a
 failure, even if the final order comes out right.
4. **It must not hire one helper per card.** A few hundred thousand elements will exhaust
 the OS's thread limit if every recursive split spawns a new thread. Bound how deep the
 recursion is allowed to keep spawning helpers before it just does the rest itself.
5. **If a comparison throws**, on whichever thread it happens to run on, the exception
 must come back out of `parallel_quick_sort` as itself, not swallowed, not turned into
 a different exception type, not a crash.

## Why the constraints exist

- **`T` only needs `operator<` and to be movable.** The tests use plain `int` and two
 instrumented wrapper types, not just `int`, don't assume more about `T` than the
 interface promises.
- **Don't assume a particular launch policy is chosen for you.** `std::async` can run
 your task on a new thread or defer it until `.get()`, that choice is yours to make
 explicitly, not something to leave to the implementation's default judgment.
