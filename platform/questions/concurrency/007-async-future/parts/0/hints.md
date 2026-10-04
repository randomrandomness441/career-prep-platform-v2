Partition around a pivot exactly like a sequential quicksort. The only new question is: for
each half, do you sort it here, or hand it to `std::async`? Try handing off the lower half
and sorting the upper half yourself while it runs — then `.get()` the lower half's result.
Which launch policy do you pass, and what happens to your "parallelism" if you leave it out?

---

`std::async(f, args...)` with no policy picks `std::launch::async | std::launch::deferred`
— the implementation is free to run `f` on `get()`, on the calling thread, instead of on a
new one. Nothing tells you which choice it made; the code compiles and sorts correctly
either way, and only a thread-count check would catch the difference. Name the policy you
want explicitly.

---

Recursing with `std::async` on every call spawns one thread per level, and a level has up
to N calls at the leaves — a 200,000-element input tries to create close to 200,000
threads, and `std::thread`'s constructor throws `std::system_error` once the OS's limit is
hit. Cap it with a depth counter derived from `std::thread::hardware_concurrency()`: spawn
while depth remains, sort inline once it hits zero. `std::future`'s destructor already
waits for an unfinished `std::async(launch::async, ...)` task, which is what makes it safe
for an exception unwinding past a `future` to not outlive the data the task is using.
