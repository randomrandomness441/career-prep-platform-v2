Each bucket is a lock-free, prepend-only singly linked list — the same
CAS-retry-loop-on-the-head shape as [[015-treiber-stack]]. `insert` needs to do two things:
check the key isn't already present, and publish a new node as the bucket's head. Get the
publishing half working first (always insert, don't worry about duplicates yet) before
tackling the check.

---

Run the boilerplate against the tests before trusting a check done once, before the retry
loop — under `-DSHAKE` (this course's scheduling-perturbation build) it fails reliably: two
threads can both find "key not present" true at nearly the same moment, both build a node,
and race to `compare_exchange`. One wins; the other's `compare_exchange_weak` fails and
retries with the refreshed head — but if it never re-checks for the key, it just prepends
its own node anyway, on top of the winner's. Now the bucket has two nodes with the same key.

---

Move the existence check *inside* the retry loop, so it runs against the current attempt's
snapshot of the head every time, not just once before the loop starts. If another thread's
insert of the same key won the race since your last check, the next iteration's scan (now
including that winner's node) catches it before you'd otherwise duplicate it — `return
false` from inside the loop, after `delete`-ing any node you'd already speculatively
allocated, since nothing else can have a pointer to a node that never got published via a
successful CAS.
