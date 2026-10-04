021's resource-hierarchy fix is still correct here — it's still deadlock-free, still gives
mutual exclusion on shared forks. Run it against this question's fairness check anyway: a
philosopher with zero delay between meals wins the race to re-acquire its own forks against
a neighbour who just went to sleep for a realistic think-time, over and over, because
nothing makes it wait its turn. `std::mutex` has no fairness guarantee — whichever thread
tries to lock it next is not necessarily whichever thread has been waiting longest.

---

The fix isn't in the fork-acquisition order — it's a cost every philosopher pays after
eating, before it's allowed to try again: a small backoff. Even a hungry philosopher with no
"natural" think time can be made to wait a little before its next attempt, which puts a
floor under how much of a head start it can build over neighbours who are already waiting.

---

Don't make that backoff a fixed, identical delay for everyone — if every philosopher backs
off by exactly the same amount, they can end up retrying in lockstep, all waking up and
racing for forks at the same moment, over and over, which trades runaway unfairness for a
different problem (see the reading). Add randomness (jitter) to the backoff duration so
different philosophers' retry attempts spread out instead of clustering.
