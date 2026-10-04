Write down what your loop does when `nudge()` fires at the 10 ms mark of a 50 ms wait.
If the next call is `cv_.wait_for(lk, timeout)`, how much longer can this function still
block? Now imagine a nudge every 4 ms.

The requested timeout is a *duration*. What the caller actually asked for is a *moment*.
---
Compute the moment once, before you touch the mutex:

```cpp
const auto deadline = std::chrono::steady_clock::now() + timeout;
```

Then wait until that moment rather than for a duration. `condition_variable::wait_until`
takes exactly this. Re-entering it after a wakeup sleeps only for the time that is left,
because the target has not moved.

`steady_clock` and not `system_clock`: `system_clock` is the wall clock, and it can be
stepped backwards by NTP or by a user. A `system_clock` deadline that gets jumped over is
one thing; a clock that moves *back* five minutes turns your 50 ms deadline into a five
minute one. `steady_clock` only counts forward and cannot be set.
---
```cpp
class result_slot {
    std::mutex m_;
    std::condition_variable cv_;
    std::optional<int> value_;

public:
    void set(int v) {
        {
            std::lock_guard<std::mutex> lk(m_);
            value_ = std::move(v);
        }
        cv_.notify_all();
    }

    void nudge() { cv_.notify_all(); }

    std::optional<int> wait_for_result(std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::unique_lock<std::mutex> lk(m_);
        if (!cv_.wait_until(lk, deadline, [this] { return value_.has_value(); }))
            return std::nullopt;
        return value_;
    }
};
```

The predicate overload returns the predicate's final value: `false` means the deadline
passed with the condition still unmet. It also checks the predicate *before* sleeping,
which is what makes a `0ms` timeout return an already-present value instead of failing.

Footnote worth remembering: `cv_.wait_for(lk, timeout, pred)` — the three-argument form —
would also have been correct. The standard defines it as
`wait_until(lk, steady_clock::now() + timeout, pred)`, so it fixes the deadline once
internally. The trap lives only in the two-argument `wait_for` used inside a loop you wrote
yourself.
