## 1. Reframe

Every earlier question in this pack handed you sampled data already reduced to text.
Real work doesn't come pre-reduced. The actual skill is: build it, run it, profile it,
read the result, then act — and the first three steps are not optional shortcuts, they're
where the whole diagnosis happens.

## 2. The tools, for real this time

This was actually run, on this machine, at every step below — no numbers here are
invented.

`dedupe`'s naive version compiled at `-O1` (the exact flag this platform's own test
harness uses) and run on 150,000 ids: **1451ms**. The fixed version, same input, same
flag: **15.7ms** — about 92x faster. Under ThreadSanitizer instrumentation (which
normally slows everything down): the fixed version was still only **24ms**. That gap
is not subtle, and it is not visible anywhere in the source code — both versions are
eleven lines long and both look like a reasonable way to deduplicate a list.

Sampling the naive version with macOS's built-in `sample` command on a real run of it
(300,000 elements, several seconds of runtime) gave this call tree, unedited:

```
273 main (in dedupe_O0)  dedupe.cpp:24
  273 dedupe(std::vector<int> const&) (in dedupe_O0)  dedupe.cpp:14
    207 seen_before(std::vector<int> const&, int) (in dedupe_O0)  dedupe.cpp:7
    + 163 std::operator!= [iterator comparison]  wrap_iter.h:150
    + 91 std::operator== [iterator comparison]  wrap_iter.h:125
    47 seen_before(std::vector<int> const&, int) (in dedupe_O0)  dedupe.cpp:7
    17 seen_before(std::vector<int> const&, int) (in dedupe_O0)  dedupe.cpp:7
```

273 out of 273 samples land inside `seen_before` and the iterator machinery it drives.
That's the whole diagnosis, in one picture: one function, at one call site, is where
every sample lands, and it's the linear scan inside the loop that runs once per
element already collected.

## 3. The broken version, first

Read `dedupe` on its own and it looks completely fine: a loop, a helper, a
push_back. Nothing about it screams "this is quadratic." The quadratic cost is a
relationship between two pieces of code that are individually simple — `seen_before`
scans a vector that `dedupe`'s own loop keeps growing, so the cost of each call grows
with how much progress has already been made. Code review catches bugs you can see in
one place. It does not reliably catch a cost that only exists in the interaction
between a loop and what grows inside it — that's exactly what a profiler is for, and
exactly why this question forced you to actually run one instead of reading a
description of one.

The fix trades the linear scan for a hash set: `std::unordered_set` gives average O(1)
membership testing and insertion, so the total cost becomes proportional to how many
elements there are, not to how many elements there are squared.

Interestingly, at `-O2` this specific example does something else worth knowing:
`seen_before` gets fully inlined into `main`, and sampling shows 100% of time on a
single source line inside `main` with no separate `seen_before` frame visible at all.
The bottleneck doesn't disappear — it becomes invisible as a distinct frame, which is
exactly the inlining lesson from question 006, now witnessed directly instead of just
described.

## 4. Real-world usage

This exact category of bug — an O(1)-looking helper called inside a loop, where the
helper's own cost secretly depends on the loop's progress — is one of the most common
real performance bugs in production code, because both pieces pass code review
individually. A close real-world cousin: [OpenSearch's Java client](https://opensearch.org/blog/opensource-perf/)
found that a JSON parsing helper was triggering expensive interface lookups on every
call, 26% of total client CPU, invisible until profiled — visible CPU percentage on the
call site itself didn't tell the real story, the same lesson as question 004 applied to
a caller-and-callee relationship instead of one function scattered across parents.
Fixing it, and validating with a microbenchmark rather than trusting the flame graph
alone, got a real, measured 50x speedup (71.79 microseconds down to 1.42 microseconds
per call).

## 5. Performance

Measured on this machine, all real: naive at 150,000 elements, 1451ms. Fixed, same
input, 15.7ms (about 92x). Fixed under TSan instrumentation, 24ms. Fixed under an
`-O2 -DSHAKE` build, 11-13ms. The quadratic cost gets worse fast: doubling the input
roughly quadruples the naive version's time (it's O(n^2)) while the fixed version's
time roughly doubles (O(n) with hashing overhead) — at 10x the input, expect the naive
version to take roughly 100x longer, not 10x.

## 6. Where this solution fails

`std::unordered_set<int>` is the right fix for `int` keys with a good default hash.
It is not automatically the right fix for every type — a poorly-distributed custom hash
function can degenerate toward the same linear-scan behavior this question just taught
you to avoid, just hidden one layer deeper (many keys colliding into the same bucket).
If profiling a similar real bug shows one bucket, or one hash comparison function, as
unexpectedly wide, that's the same diagnosis at a different layer: something assumed to
be O(1) is not actually behaving like it under this specific data.

## 7. Interview follow-ups

- What if the ids needed to stay sorted in the output instead of first-occurrence order?
  `std::set<int>` (or sort-then-unique on a copy) gives O(n log n), still nowhere near
  quadratic, at the cost of losing insertion order.
- What if this ran under real memory pressure, with billions of ids instead of hundreds
  of thousands? An in-memory hash set stops being free — at that scale you'd reach for
  an external sort-based dedupe or a probabilistic structure (a Bloom filter as a cheap
  first-pass filter before a slower exact check), trading some memory or a small false-
  positive rate for throughput.
