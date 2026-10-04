Run the boilerplate against the tests before changing anything — it hangs. All five
philosophers grab their left fork first, and if all five do that at once, every fork is now
held by someone waiting for their neighbour's fork. Five threads, each one lock away from
finishing, each blocked on the thread next to it. What has to be true of a set of
"waiting for" relationships for a deadlock like this to exist, and can you make that
impossible instead of unlikely?

---

The four classic deadlock conditions all have to hold at once: mutual exclusion, hold-and-
wait, no preemption, and circular wait. You can't remove the first three here — a fork can't
be shared, a philosopher keeps their first fork while reaching for the second, and nobody
can be forced to drop a fork. The one you *can* remove is circular wait: if every thread that
ever needs two locks always acquires them in the same global order, a cycle can't close,
because closing it would require someone to acquire a lock "backwards" relative to that
order.

---

Fork `i` and fork `(i+4) % 5` are a philosopher's left and right. For philosophers 1–4,
left > right already, so left-then-right is already descending index order. Philosopher 0 is
the one seat where left (fork 0) is *smaller* than right (fork 4) — taking left first there
means taking the smallest index first, breaking the pattern every other seat follows.
Special-case exactly that one philosopher to take their forks in the opposite order, and
every philosopher in the room is now acquiring forks in strictly descending index order, with
no exceptions.
