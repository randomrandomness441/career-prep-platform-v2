You do not need to match up specific threads. Nobody cares *which* hydrogen pairs with
*which* oxygen — only how many of each have already gone out since the last complete
molecule.

So keep two integers under the mutex: `h_` and `o_`, both meaning "already released as part
of the molecule currently being assembled". A hydrogen may go when `h_ < 2`. An oxygen may
go when `o_ < 1`. When the molecule is finished, both drop back to zero and everyone
waiting gets another look.
---
Write the wait as a predicate wait, not a bare `wait()`:

```cpp
std::unique_lock<std::mutex> lk(m_);
cv_.wait(lk, [this] { return h_ < 2; });
```

Now the important question, which is the whole exercise: **where does
`releaseHydrogen()` go?**

If you call it after unlocking, or before you increment `h_`, ask yourself what the next
thread sees in the gap. It sees a counter that has not been updated yet, decides it has
room, and emits its atom. Your counters will be flawless and your output will be `HHH`.

The release and the increment have to be one indivisible step, which means both happen
while you hold the lock.
---
```cpp
class H2O {
    std::mutex m_;
    std::condition_variable cv_;
    int h_ = 0, o_ = 0;

public:
    void hydrogen(std::function<void()> releaseHydrogen) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return h_ < 2; });
        releaseHydrogen();                     // inside the lock, on purpose
        ++h_;
        if (h_ == 2 && o_ == 1) { h_ = 0; o_ = 0; }
        cv_.notify_all();
    }

    void oxygen(std::function<void()> releaseOxygen) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return o_ < 1; });
        releaseOxygen();
        ++o_;
        if (h_ == 2 && o_ == 1) { h_ = 0; o_ = 0; }
        cv_.notify_all();
    }
};
```

`notify_all` and not `notify_one`: completing a molecule can make **two** hydrogens
runnable at once, and `notify_one` wakes exactly one of them. The other stays parked until
somebody else happens to notify, which for the last molecule in a run is never.

Note also that there is no deadlock hiding here, given a supply of exactly 2n hydrogens and
n oxygens. Whenever hydrogens are blocked it is because `h_ == 2`, and then either `o_ == 0`
so an oxygen can go, or the molecule was already complete and the counters were reset before
anyone had a chance to block.
