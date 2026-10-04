The functional part is ordinary condition-variable code: a free-slot counter guarded by a
mutex, `acquire()` waits while it's zero, `release()` increments it and wakes someone. Get
that working first and it will pass a correctness check with either `notify_one` or
`notify_all` — the difference this question grades doesn't show up as a wrong answer.

---

`release()` opens exactly one slot. Ask: of all the threads currently asleep in
`acquire()`, how many of them can actually take that one slot? Whichever notify call you
pick, everyone it wakes has to re-lock the mutex and recheck the predicate before it can
know whether it succeeded — waking threads that can't succeed is pure overhead, not a
safety issue.

---

Every waiter in this pool is checking the exact same condition (`free_ > 0`) — there's no
"my turn specifically" the way FizzBuzz's four threads each have. When every sleeper shares
one predicate, waking exactly one of them is enough: whichever thread the OS picks will find
the predicate true, because you just made it true. Compare `notify_one()` against
`notify_all()` here and watch `wasted_wakeups`.
