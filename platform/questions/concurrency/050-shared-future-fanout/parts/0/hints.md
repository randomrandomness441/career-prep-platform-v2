`std::future<T>::get()` is documented to work exactly once: it moves the result out of the
shared state, and the future is not valid afterward. It was never designed for "many
readers" — it was designed for "the one place that consumes this result." What type in
`<future>` is explicitly meant to be read more than once, and by more than one thread?

---

`std::shared_future<T>` is what you want. You can get one either by calling `.share()` on a
`std::future<T>` (converts it, the original future becomes invalid), or by building your
async work so the future you get back is already a `shared_future` — `std::async` still
gives you a plain `future`, so you `.share()` it. Store the `shared_future` as a member, and
`get()` just calls `.get()` on it.

---

Why is it safe for many threads to call `get()` on the *same* `shared_future` object at the
same time, when it isn't safe to do that on a plain `future`? Because `shared_future::get()`
is `const` and returns `const T&` (or a copy) — it never mutates the shared state or the
object, it only reads already-published data. Reading the same immutable thing from many
threads at once needs no lock. `std::async(..., work).share()` in the constructor is the
whole implementation; `get()` is one line: `return fut_.get();`.
