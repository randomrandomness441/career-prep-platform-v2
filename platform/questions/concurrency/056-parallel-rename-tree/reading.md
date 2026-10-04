## 1. Reframe the problem

A sequential recursive file-count is usually written with an accumulator: a running total
passed down (or referenced) through the recursion, incremented as each file is found. That
pattern translates cleanly to parallel recursion in every way except one, the accumulator
stops being one thread's private running total and becomes shared, mutable state touched by
every concurrently-running branch of the recursion at once. The fix isn't a smarter counter,
it's recognizing that **recursive divide-and-conquer already gives you a place to put the
answer that doesn't need to be shared: the return value.** Each call already computes its
own count and hands it back to its caller; the caller already visits every child's result to
join them together (that's what `t.join()` already forces it to wait for). Summing return
values there is free, no new synchronization needed, because nothing is shared.

## 3. The broken version, first

The boilerplate threads a shared `int& total` through the whole recursion:

```cpp
for (auto& f : node.files) {
    f = rename_fn(f);
    ++total; // shared across every thread, at every level
}
```

**Why it looks right:** it's the direct parallel translation of the sequential version most
people would write first, pass an accumulator down, increment it wherever work happens.
The pattern is completely natural for a *single-threaded* recursive traversal; the bug only
exists once sibling subtrees are running on different threads at the same time, each with
its own view of "add one more."

Running it, a 1,705-file tree, branching factor 4, 4 levels deep:

```
parallel_rename_all() returned 1689, expected 1705 -- the count is wrong
under concurrent renaming (file names ARE renamed correctly, but the
shared counter lost updates)
```

Worth noticing precisely what's wrong and what isn't: every file **was** renamed correctly,
`rename_fn` runs once per file regardless of the counter bug, since each thread only ever
touches its own node's `files` vector, never another thread's. Only the *count* is wrong,
because it's the one piece of state every thread actually shares.

The fix removes the shared state entirely:

```cpp
std::vector<int> sub_counts(node.subdirs.size(), 0);
threads.emplace_back([sub, &rename_fn, &sub_counts, i] {
    sub_counts[i] = parallel_rename_all(*sub, rename_fn); // this thread's OWN slot
});
...
for (int c : sub_counts) count += c;
```

Same 1,705-file tree: exact count, every run.

## 6. Where this solution fails

- **One OS thread per subdirectory, at every level, with no cap.** A wide or deep enough
 tree spawns proportionally many threads, this exercise's test tree (branching 4, depth 4)
 already spawns around 340 of them. A real filesystem tree can be far wider; past some
 point, thread-creation overhead and OS thread limits (empty-thread cost was measured at
 ~36.5 µs each, back in this course's very first chapter) dominate, and the recursion needs
 a depth cutoff (recurse sequentially, or hand off to a bounded thread pool, once remaining
 work is small), the same "don't launch one thread per unit of tiny work" lesson
 [[034-parallel-argsort]] and this course's earliest exercises both cover.
- **This never touches a real filesystem**, on purpose, to stay deterministic, a real
 implementation renaming actual files needs to handle a rename that fails partway (a
 permissions error, a name collision with an existing file, the process being killed
 mid-tree), and decide what "partially renamed" means for the caller, which this in-memory
 version has no analog for at all.
- **No detection of two files that would collide after renaming**, if `rename_fn` maps two
 different original names to the same new name within one directory, this design applies
 both renames without complaint (in memory, "renaming" is just replacing a string; on a
 real filesystem, the second rename would silently overwrite the first file).
- **The whole tree is walked and renamed unconditionally.** A design that needed to filter
 (rename only files matching some pattern) or needed a dry-run mode (report what *would* be
 renamed without mutating anything) would need to thread that decision through the same
 recursion this exercise builds, not covered here.

## 7. Interview follow-ups

**"You said the file names came out correct even with the buggy counter, why exactly?"**
Because renaming and counting are two different operations on two different kinds of state:
each thread's `for (auto& f : node.files)` loop only ever mutates *that node's own*
`files` vector, which no other thread ever touches (each `DirNode` belongs to exactly one
recursive call). The counter was the only thing actually shared across threads, which is
exactly why removing the sharing (return values instead of a shared accumulator) fixes it
without touching the renaming logic at all.

**"Extreme case, a million files, mostly in one giant directory rather than spread across
subdirectories. Does this design still parallelize well?"** No, `parallel_rename_all`
only parallelizes across *subdirectories*; a single directory's own file list is renamed by
one thread, sequentially, no matter how many files it has. A tree that's mostly flat (one
huge directory, few subdirectories) gets almost no benefit from this design; parallelizing
within a single directory's file list as well (splitting `node.files` into chunks, the way
[[007-async-future]]'s quicksort splits its own array) would be needed to help that shape.

**"How would you extend this to actually call the real rename() syscall, and what changes
about error handling?"** Each `rename_fn(f)` becomes something that can genuinely fail (a
permissions error, a target that already exists, a full disk), the count-only return value
here would need to become richer (which files succeeded, which failed and why), most
naturally as a small result struct per file rather than a single aggregate int, following
the same "collect all outcomes, not just a count" idea [[055-async-gather]]'s reading raises
for exception handling across many async results.
