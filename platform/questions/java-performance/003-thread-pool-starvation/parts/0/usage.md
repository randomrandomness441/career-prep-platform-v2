Plain text, three numbered points. Example shape:

```
1. Starvation. The threads exist and are all accounted for -- the dump shows
   them WAITING inside a downstream call, not "pool has no thread to give
   out." Exhaustion would look like requests rejected or queued with no
   thread state to inspect at all.
2. Almost nothing -- CPU is under 10%, the threads aren't computing, they're
   parked. Off-CPU analysis (or here, just the thread dump) is what actually
   shows why they're not running.
3. More threads means more requests can pile up waiting on the same slow
   pricing service at once, adding load to something already struggling --
   it doesn't fix the dependency, it just lets more requests get stuck behind
   it. A timeout + circuit breaker on that call is the real fix.
```
