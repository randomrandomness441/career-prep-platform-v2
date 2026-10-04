Count the *conditions* in the problem, not the operations. A producer parks because "the
queue is full". A consumer parks because "the queue is empty". Those are two different
statements about the world, and a thread waiting for one has nothing useful to do when the
other becomes true.

Now ask what `cv.notify_one()` does when both kinds of thread are parked on the same
condition variable. It picks one. It has no idea which condition you meant.
---
Two condition variables, one per condition:

```cpp
std::condition_variable not_full_;    // producers park here
std::condition_variable not_empty_;   // consumers park here
```

`enqueue` waits on `not_full_` and signals `not_empty_`. `dequeue` waits on `not_empty_`
and signals `not_full_`. Now every notification lands on a thread that was waiting for
exactly the thing that just happened, so `notify_one` cannot be wasted.

Two more details:

- **Unlock before you notify.** `lk.unlock(); not_empty_.notify_one();` — otherwise the
  thread you just woke returns from `wait()` and immediately blocks on the mutex you are
  still holding.
- **Shutdown.** A parked thread only re-checks its predicate when notified, so `close()`
  has to (a) put something in *both* predicates that means "stop waiting", and (b)
  `notify_all` on both variables. One flag, mentioned in both predicates.
---
```cpp
class bounded_queue {
    mutable std::mutex m_;
    std::condition_variable not_full_, not_empty_;
    std::queue<int> q_;
    const std::size_t capacity_;      // size_t, not int
    std::atomic<bool> closed_{false};

public:
    explicit bounded_queue(std::size_t capacity)
        : capacity_(capacity == 0 ? 1 : capacity) {}

    bool enqueue(int v) {
        std::unique_lock<std::mutex> lk(m_);
        not_full_.wait(lk, [this] { return q_.size() < capacity_ || closed_.load(); });
        if (closed_.load()) return false;
        q_.push(std::move(v));
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }

    std::optional<int> dequeue() {
        std::unique_lock<std::mutex> lk(m_);
        not_empty_.wait(lk, [this] { return !q_.empty() || closed_.load(); });
        if (q_.empty()) return std::nullopt;     // closed and drained
        int v = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    void close() {
        { std::lock_guard<std::mutex> lk(m_); closed_.store(true); }
        not_full_.notify_all();
        not_empty_.notify_all();
    }
};
```

Read `dequeue`'s exit condition again: it checks `q_.empty()`, **not** `closed_`. A closed
queue that still holds items keeps delivering them. Checking `closed_` first would throw
away everything buffered at shutdown, which is almost never what you want.

And read `close()` again. `closed_` is atomic, so why take the mutex at all? Because the
atomic protects the *value*, not the *timing*. Without the lock, a consumer can evaluate
its predicate as false and then be interrupted before it parks; if `close()` runs in that
gap, its `notify_all` reaches an empty waiting room and the consumer parks a microsecond
later, forever. Taking the mutex makes "evaluate the predicate" and "set the flag" unable
to overlap.
