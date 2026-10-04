Plain text, three numbered points. Example shape:

```
1. Contention needs two or more threads actually racing for the same seed
   at once -- a single thread never fails a CAS attempt since nothing races
   it. A single-threaded test, however many iterations, never reveals this.
2. A thread blocked on a held lock parks and stops burning CPU. A thread
   whose CAS fails immediately retries -- real, wasted CPU work, and more
   competing threads means more failures, means the next attempt is more
   likely to fail too. Lock cost is mostly waiting; failed-CAS cost is
   mostly repeated wasted work that gets worse as contention grows.
3. No, threads get independently seeded on purpose, specifically to avoid
   correlated sequences. The real drop-in caveat: you can't set an explicit
   seed on it the way you can with new Random(seed), so code relying on a
   fixed shared seed for reproducible test output loses that when switching.
```
