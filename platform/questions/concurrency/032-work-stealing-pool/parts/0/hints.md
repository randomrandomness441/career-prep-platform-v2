Get one worker's own queue working first: `submit` distributes tasks round-robin across
`num_threads` queues (one per worker), each guarded by its own mutex. A worker's main loop
tries its own queue first. Only once that's solid does "stealing" matter: when a worker's
own queue is empty, it should try every other worker's queue before concluding there's
nothing to do anywhere.

---

Run the boilerplate against the tests before trusting that the shutdown logic is fine — it
crashes. `std::thread`'s destructor calls `std::terminate()` if the thread object is still
*joinable* (hasn't been joined or detached) when it's destroyed. A pool whose destructor sets
a stop flag but never calls `.join()` on its worker threads destroys those `std::thread`
objects while the threads are still running — every one of them still joinable.

---

The fix is one loop in the destructor: `for (auto& t : workers_) t.join();`, after setting
whatever flag tells the workers' loops to actually exit. Order matters — set the stop signal
*first*, so every worker's loop has a way to know it should eventually return, then join,
which blocks until each one actually does.
