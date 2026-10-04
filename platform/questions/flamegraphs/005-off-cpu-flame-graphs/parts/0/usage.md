Plain text, three numbered points. Example shape:

```
1. A CPU flame graph only samples threads actually running on a core. Time
   spent blocked (waiting on a lock, disk, network, sleep) costs zero CPU, so
   it's invisible no matter how long the wait is. If the regression is "more
   waiting," not "more computing," the CPU graph has nothing new to show.
2. Off-CPU flame graph would show it. The top frame would be the lock/wait
   call the blocked thread is sitting inside (a futex wait, a mutex lock),
   sized by how long threads stayed there, not what they computed.
3. No. The blind spot is a category problem, not a duration problem -- a CPU
   graph never samples off-CPU time regardless of capture length. Longer just
   means more of the same kind of sample, not a new kind.
```
