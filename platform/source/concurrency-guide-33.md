# C++ Concurrency Master Interview Guide (33 Questions)

> Sourav's original question bank, pasted 2026-09-08 after `platform/source/`
> was found deleted. This file is THE authority for statements, model answers
> and follow-ups of the remaining guide-derived questions (022, 025–047).
> NOTE: LLM-generated, contains verified bugs — see the audit list in
> `platform/AUTHORING.md`. Re-verify every solution by running it.

## Part 1: Core Synchronization Problems (LeetCode Style)

### 1. 1114. Print in Order
**Problem:** 3 threads call `first`, `second`, `third`. Ensure output is always "firstsecondthird".
**Optimal Code:**
```cpp
#include <functional>
#include <semaphore>
class Foo {
    std::binary_semaphore sem2{0}; 
    std::binary_semaphore sem3{0};
public:
    Foo() {}
    void first(std::function<void()> printFirst) {
        printFirst();
        sem2.release(); 
    }
    void second(std::function<void()> printSecond) {
        sem2.acquire(); 
        printSecond();
        sem3.release(); 
    }
    void third(std::function<void()> printThird) {
        sem3.acquire(); 
        printThird();
    }
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: What happens if a user accidentally calls `first()` twice? What happens on the second `sem2.release()`?**
    *   *A:* A `std::binary_semaphore` can only hold a max value of 1. Calling `release()` twice causes the internal counter to overflow to 2. This is **Undefined Behavior** in C++20. In practice, it may throw `std::system_error` or silently corrupt the state, causing future `acquire()` calls to incorrectly pass through without waiting.
    *   *Fix:* Use a `std::counting_semaphore<2>` if double-calling is expected, or guard the function with an `std::atomic<bool> has_run{false}`.
2.  **Q: Let's say an exception is thrown inside `printFirst()`. Will `second()` ever run? How do you fix it?**
    *   *A:* No. If `printFirst()` throws, `sem2.release()` is never called, causing `second()` to deadlock forever waiting on the semaphore.
    *   *Fix:* Wrap the call in `try { printFirst(); } catch(...) { sem2.release(); throw; }` to ensure the semaphore is signaled even if the print fails, allowing the system to gracefully unwind.
3.  **Q: How would you scale this to 100 functions printing in order, without creating 99 semaphores?**
    *   *A:* Use the Condition Variable + State pattern. One `std::mutex`, one `std::condition_variable`, and an `int turn = 1`.
    ```cpp
    std::mutex m;
    std::condition_variable cv;
    int turn = 1;
    void func(int my_id, std::function<void()> printFunc) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [this, my_id]() { return turn == my_id; });
        printFunc();
        turn++;
        cv.notify_all();
    }
    ```
    *   *Drawback:* `notify_all()` wakes up all 99 waiting threads (Thundering Herd), though only 1 proceeds. For true high-performance scaling, a chain of `std::binary_semaphore`s is actually O(1) per thread, making it faster despite higher memory usage.

### 2. 1115. Print FooBar Alternately
**Problem:** Two threads, one prints "Foo", the other "Bar". Print "FooBar" N times.
**Optimal Code:**
```cpp
class FooBar {
    int n;
    std::mutex m;
    std::condition_variable cv;
    bool foo_turn = true;
public:
    FooBar(int n) : n(n) {}
    void foo(function<void()> printFoo) {
        for (int i = 0; i < n; i++) {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [this]() { return foo_turn; });
            printFoo();
            foo_turn = false;
            cv.notify_one();
        }
    }
    void bar(function<void()> printBar) {
        for (int i = 0; i < n; i++) {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [this]() { return !foo_turn; });
            printBar();
            foo_turn = true;
            cv.notify_one();
        }
    }
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: Why use `notify_one()` instead of `notify_all()`?**
    *   *A:* Since there are only two threads, and they strictly alternate, only one thread is ever waiting at a time. `notify_all()` is an anti-pattern here because it forces the OS to wake up all waiting threads, causing unnecessary context switches and CPU waste (Thundering Herd).
2.  **Q: Can we solve this without a mutex using C++20 atomics?**
    *   *A:* Yes, using `std::atomic<bool>` with a busy-wait loop: `while(foo_turn.load(std::memory_order_acquire));`. 
    *   *Why is this bad?* It causes **thread starvation/cpu burning**. The thread never goes to sleep; it consumes 100% of a CPU core continuously checking the flag. `cv.wait()` yields the CPU to the OS scheduler.
3.  **Q: Is the provided code the absolute best performance-wise? What about `std::binary_semaphore`?**
    *   *A:* Semaphores are actually faster than Mutex+CV for this specific problem because they avoid the heavy lock guard overhead. 
    ```cpp
    std::binary_semaphore foo_sem{1}, bar_sem{0};
    // foo: foo_sem.acquire(); printFoo(); bar_sem.release();
    // bar: bar_sem.acquire(); printBar(); foo_sem.release();
    ```

### 3. 1116. Print ZeroEvenOdd
**Problem:** 3 threads: Zero, Even, Odd. Print "01020304..." up to n.
**Optimal Code (Semaphores):**
```cpp
#include <semaphore>
class ZeroEvenOdd {
    int n;
    std::binary_semaphore sem_zero{1}, sem_even{0}, sem_odd{0};
public:
    ZeroEvenOdd(int n) : n(n) {}
    void zero(function<void(int)> printNumber) {
        for (int i = 1; i <= n; i++) {
            sem_zero.acquire();
            printNumber(0);
            if (i % 2 == 0) sem_even.release();
            else sem_odd.release();
        }
    }
    void even(function<void(int)> printNumber) {
        for (int i = 2; i <= n; i += 2) {
            sem_even.acquire();
            printNumber(i);
            sem_zero.release();
        }
    }
    void odd(function<void(int)> printNumber) {
        for (int i = 1; i <= n; i += 2) {
            sem_odd.acquire();
            printNumber(i);
            sem_zero.release();
        }
    }
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: What is the fundamental difference between a Mutex and a Binary Semaphore?**
    *   *A:* **Ownership.** A mutex must be unlocked by the *exact same thread* that locked it. A semaphore has no ownership concept; Thread A can acquire it, and Thread B can release it. In this code, `sem_zero` is acquired by the `zero` thread but released by `even` and `odd`. This is impossible with a `std::mutex`.
2.  **Q: If n is 100,000, what is the memory overhead of this solution?**
    *   *A:* Minimal. 3 `std::binary_semaphore` objects (typically 4 bytes each on Linux). The loops use stack variables.
3.  **Q: Could a spurious wakeup break this semaphore solution?**
    *   *A:* No. Semaphores track their state internally via atomic counters. If a thread wakes up spuriously, the `acquire()` will see the counter is still 0 and put the thread back to sleep. (Note: this is true for `std::counting_semaphore` in C++20, but if you implemented semaphores manually using CVs, you *would* need a predicate).

### 4. 1117. Building H2O
**Problem:** Threads release H and O. Form H2O (2 H's, 1 O per molecule).
**Optimal Code (Barrier/Counters):**
```cpp
class H2O {
    std::mutex m;
    std::condition_variable cv;
    int h = 0, o = 0;
public:
    void hydrogen(function<void()> releaseHydrogen) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [this]() { return h < 2; });
        h++;
        releaseHydrogen();
        check_reset();
    }
    void oxygen(function<void()> releaseOxygen) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [this]() { return o < 1; });
        o++;
        releaseOxygen();
        check_reset();
    }
private:
    void check_reset() {
        if (h == 2 && o == 1) {
            h = 0; o = 0;
            cv.notify_all();
        }
    }
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: If 1000 Hydrogen threads arrive and 0 Oxygen threads arrive, what happens to memory?**
    *   *A:* Only 2 Hydrogen threads enter the critical section. The remaining 998 are parked in the `cv.wait()` queue (managed by the OS). Memory is safe, but the program is effectively deadlocked until an Oxygen arrives.
2.  **Q: How do you prevent thread starvation if the OS keeps scheduling Hydrogens infinitely?**
    *   *A:* The current code inherently prevents starvation. Once 2 H's arrive, `h` becomes 2. Any subsequent H thread fails the predicate `h < 2` and sleeps. This forces the system to wait for an O.
3.  **Q: Is this code truly deterministic about which H's bond with which O's?**
    *   *A:* No. The OS schedules threads arbitrarily. If 100 H's are waiting, when an O arrives and resets, the OS picks 2 random H's to wake up next. If strict FIFO ordering is required, you need a queue-based ticket system.

### 5. 1188. Design Bounded Blocking Queue
**Problem:** Thread-safe queue with max capacity. Block on push if full, block on pop if empty.
**Optimal Code:**
```cpp
class BoundedBlockingQueue {
    std::queue<int> q;
    int capacity;
    std::mutex m;
    std::condition_variable not_full, not_empty;
public:
    BoundedBlockingQueue(int capacity) : capacity(capacity) {}
    void enqueue(int element) {
        std::unique_lock<std::mutex> lk(m);
        not_full.wait(lk, [this]() { return q.size() < capacity; });
        q.push(element);
        not_empty.notify_one();
    }
    int dequeue() {
        std::unique_lock<std::mutex> lk(m);
        not_empty.wait(lk, [this]() { return !q.empty(); });
        int val = q.front(); q.pop();
        not_full.notify_one();
        return val;
    }
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: Why do we need two condition variables?**
    *   *A:* If we used one, `notify_one()` might wake up another Producer instead of a Consumer. The Producer would wake up, check the `not_full` predicate, see the queue is still full, and go back to sleep. The Consumer would never be notified, causing a logical deadlock or severe performance degradation.
2.  **Q: In `enqueue`, I call `notify_one()` before the mutex is unlocked (at the end of the function). Is this a problem?**
    *   *A:* It's not a correctness bug, but a performance issue. The woken Consumer thread immediately tries to lock the mutex, but the Producer is still holding it. The Consumer context-switches back to sleep, then the Producer unlocks, then the Consumer wakes again. 
    *   *Fix:* Call `lk.unlock()` before `notify_one()` to allow immediate handoff.
3.  **Q: How do you gracefully shut down this queue if threads are blocked on `wait()`?**
    *   *A:* Add a `std::atomic<bool> shutdown{false};`. Change predicates: `[this]() { return !q.empty() || shutdown; }`. When `shutdown` is true, throw an exception or return an error code instead of pushing/popping.

### 6. 1195. Fizz Buzz Multithreaded
**Problem:** 4 threads print Fizz, Buzz, FizzBuzz, or Number for 1 to n.
**Optimal Code (CV + State):**
```cpp
class FizzBuzz {
    int n;
    std::mutex m;
    std::condition_variable cv;
    int current = 1;
public:
    FizzBuzz(int n) : n(n) {}
    void fizz(function<void()> printFizz) {
        while (true) {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [this]() { return current > n || (current % 3 == 0 && current % 5 != 0); });
            if (current > n) return;
            printFizz(); current++; cv.notify_all();
        }
    }
    // buzz, fizzbuzz, number follow identical pattern with different modulo checks
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: This solution uses `notify_all()`. What is the performance problem here at `n = 1,000,000`?**
    *   *A:* The **Thundering Herd Problem**. Every time a number increments, all 3 other threads wake up, fight for the mutex, check the predicate, fail, and go back to sleep. That's 4,000,000 context switches.
2.  **Q: How do you optimize it to use `notify_one()`?**
    *   *A:* You need 4 separate `std::condition_variable` objects (one for each thread type). After printing, the current thread calculates who goes next: `if ((current+1) % 15 == 0) cv_fizzbuzz.notify_one();` etc. This ensures exactly 1 thread is woken up.
3.  **Q: Why not just use 4 semaphores?**
    *   *A:* You can. But semaphores don't track the `current` variable naturally. You'd have to wrap the semaphore logic in a way that defeats their simplicity. The CV approach centralizes the state.

### 7. 1226. The Dining Philosophers
**Problem:** 5 philosophers, 5 forks. Eat without deadlocking.
**Optimal Code (Resource Hierarchy):**
```cpp
class DiningPhilosophers {
    std::mutex forks[5];
public:
    void wantsToEat(int philosopher, function<void()> pickLeftFork, function<void()> pickRightFork, function<void()> eat, function<void()> putLeftFork, function<void()> putRightFork) {
        int left = philosopher;
        int right = (philosopher + 1) % 5;
        if (philosopher == 4) {
            std::lock_guard<std::mutex> lk_right(forks[right]);
            std::lock_guard<std::mutex> lk_left(forks[left]);
            pickLeftFork(); pickRightFork(); eat(); putLeftFork(); putRightFork();
        } else {
            std::lock_guard<std::mutex> lk_left(forks[left]);
            std::lock_guard<std::mutex> lk_right(forks[right]);
            pickLeftFork(); pickRightFork(); eat(); putLeftFork(); putRightFork();
        }
    }
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: What are the 4 necessary conditions for Deadlock?**
    *   *A:* Mutual Exclusion, Hold and Wait, No Preemption, Circular Wait. Our solution breaks Circular Wait by enforcing a strict numerical ordering of fork acquisition.
2.  **Q: Does this solution maximize throughput?**
    *   *A:* It's good (allows 2 philosophers to eat concurrently), but not optimal. Philosopher 4 grabbing fork 0 first means Philosopher 0 might starve even though fork 4 is available.
    *   *Better Solution:* Use a waiter (Concierge) - a semaphore initialized to 4. A philosopher must ask the waiter for permission before picking up *any* forks. This guarantees at least 1 philosopher can always eat.
3.  **Q: If a philosopher drops a fork, can another pick it up immediately?**
    *   *A:* Yes, `std::lock_guard` releases the mutex in its destructor. The OS immediately schedules any thread blocked on that mutex.

### 8. 1242. Web Crawler Multithreaded
**Problem:** Crawl URLs concurrently, avoid visiting same URL twice.
**Optimal Code (Thread Pool + Shared State):**
```cpp
class Solution {
    std::mutex m;
    std::condition_variable cv;
    std::queue<std::string> q;
    std::unordered_set<std::string> visited;
    int active_workers = 0;
    bool done = false;
public:
    vector<string> crawl(string startUrl, HtmlParser htmlParser) {
        q.push(startUrl);
        visited.insert(startUrl);
        auto worker = [&]() {
            while (true) {
                std::unique_lock<std::mutex> lk(m);
                cv.wait(lk, [this]() { return !q.empty() || done; });
                if (done && q.empty()) return;
                std::string url = q.front(); q.pop();
                active_workers++;
                lk.unlock();
                
                vector<string> urls = htmlParser.getUrls(url);
                
                lk.lock();
                for (auto& u : urls) {
                    if (visited.find(u) == visited.end()) {
                        visited.insert(u); q.push(u);
                    }
                }
                active_workers--;
                if (q.empty() && active_workers == 0) done = true;
                cv.notify_all();
            }
        };
        std::vector<std::thread> threads;
        for (int i = 0; i < 10; i++) threads.emplace_back(worker);
        for (auto& t : threads) t.join();
        return vector<string>(visited.begin(), visited.end());
    }
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: Why do we track `active_workers`?**
    *   *A:* To know when we are truly finished. If the queue is empty but a worker is currently making an HTTP request (`getUrls`), it might return with 10 new URLs to add. If we signaled `done` just because the queue was empty, we would prematurely terminate and miss URLs.
2.  **Q: What happens if `htmlParser.getUrls()` throws an exception?**
    *   *A:* The thread crashes, `active_workers` is never decremented, `done` never becomes true, and the program deadlocks forever at `join()`.
    *   *Fix:* Wrap the HTTP call in `try/catch`. Ensure `active_workers--` happens in a `finally`-like block (using RAII or a lambda).
3.  **Q: Is `std::unordered_set` thread-safe for concurrent reads?**
    *   *A:* Yes, concurrent reads are safe *only if* no other thread is writing to it. Since we hold the mutex `m` while inserting into `visited`, and we don't read `visited` outside the mutex, it is safe.

### 9. 1279. Traffic Light Controlled Intersection
**Problem:** 4-way intersection. N/S go together, E/W go together.
**Optimal Code:**
```cpp
class TrafficLight {
    std::mutex m;
    std::condition_variable cv;
    bool is_green_ns = true;
    int current_passing = 0;
public:
    void carArrived(int carId, int roadId, int direction, function<void()> turnGreen, function<void()> crossCar) {
        std::unique_lock<std::mutex> lk(m);
        bool wants_ns = (roadId == 1 || roadId == 2);
        cv.wait(lk, [this, wants_ns]() { return (wants_ns && is_green_ns) || (!wants_ns && !is_green_ns); });
        
        if (wants_ns && !is_green_ns) { is_green_ns = true; turnGreen(); }
        if (!wants_ns && is_green_ns) { is_green_ns = false; turnGreen(); }
        
        current_passing++;
        crossCar();
        current_passing--;
        
        if (current_passing == 0) cv.notify_all();
    }
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: Why do we track `current_passing` instead of just toggling the light after 1 car passes?**
    *   *A:* To allow concurrency. Multiple cars from N and S can pass simultaneously. If we toggled after 1 car, it would be a strict 1-at-a-time intersection, defeating the purpose of a traffic light.
2.  **Q: How do you prevent starvation if North/South traffic is endless?**
    *   *A:* The current code starves East/West if N/S never stop. 
    *   *Fix:* Add a max wait time or max car count. `cv.wait_for(...)` or track `cars_passed_this_cycle++; if (cars_passed_this_cycle > 10) force_switch = true;`.
3.  **Q: If an emergency vehicle arrives, how do you preempt?**
    *   *A:* Interrupt the CV wait. Set an `emergency` flag. Force all threads to yield. Change the light exclusively for the emergency vehicle.

### 10. 3568. Minimum Moves to Clean the Classroom
**Problem:** Multiple robots cleaning a grid without stepping on the same tile.
**Optimal Code (Grid Locking):**
```cpp
class RobotCleaner {
    int rows, cols;
    std::vector<std::vector<std::mutex>> cell_locks;
public:
    RobotCleaner(int r, int c) : rows(r), cols(c), cell_locks(r, std::vector<std::mutex>(c)) {}
    void moveAndClean(int target_r, int target_c) {
        std::unique_lock<std::mutex> lk(cell_locks[target_r][target_c]);
        // Clean the cell
    }
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: If the robot needs to move from (0,0) to (0,1), should it hold the lock for (0,0) while trying to lock (0,1)?**
    *   *A:* **No!** That is a classic deadlock (Hold and Wait). Robot A holds (0,0) wants (0,1). Robot B holds (0,1) wants (0,0).
    *   *Fix:* Release the current cell, then lock the next cell. Or, use `std::lock(cell_locks[r1][c1], cell_locks[r2][c1])` to lock both simultaneously without deadlock.
2.  **Q: If a robot's battery dies while holding a mutex, what happens?**
    *   *A:* Deadlock. The mutex is never released.
    *   *Fix:* Use a `std::timed_mutex` with `try_lock_for`. If it fails, the robot is considered dead and the path is abandoned.
3.  **Q: Is locking individual cells too fine-grained?**
    *   *A:* Yes. Mutexes take ~40 bytes each. A 1000x1000 grid would use 40MB just for mutexes, and cache thrashing would destroy performance.
    *   *Better Way:* Divide the grid into larger blocks (e.g., 10x10 chunks) and lock chunks, or design a lock-free pathing algorithm where robots claim cells via atomic CAS.

## Part 2: System Design Scenarios

### 11. Concurrent LLM Serving
**Problem:** Serve AI answers to 100 concurrent users. GPU processes in batches.
**Solution:** Use a batching dispatcher thread. It collects requests for max 50ms or batch size 32, sends to GPU, distributes output via `std::promise`.
**Rigorous Follow-ups & Answers:**
1.  **Q: How do you handle client disconnect mid-stream?**
    *   *A:* The GPU calculation cannot be interrupted. Use a `std::atomic<bool> is_connected` checked per token generation. If false, discard the output and do not fulfill the promise.
2.  **Q: How do you protect the GPU KV Cache?**
    *   *A:* Read-write locks (`std::shared_mutex`). Reads during token generation are concurrent. Eviction of old context requires an exclusive write lock to prevent reading corrupted memory.
3.  **Q: If one request has 10,000 tokens and others have 10, how do you prevent head-of-line blocking?**
    *   *A:* Token bucket rate limiting or segmenting the large request into smaller sub-batches.

### 12. Ticket Reservation Locking
**Problem:** Book concert seats concurrently.
**Solution:** Use Optimistic Concurrency Control (OCC). `UPDATE seats SET user=X WHERE seat=5 AND version=1`.
**Rigorous Follow-ups & Answers:**
1.  **Q: When is the possibility of the same user calling `reserve_cb()` twice?**
    *   *A:* Network timeout. User clicks twice.
    *   *Fix:* Idempotency keys. Use a Redis `SETNX` on a UUID. If the UUID already exists, return the cached result instead of re-booking.
2.  **Q: What is a deadlock in a database context?**
    *   *A:* Tx A locks Seat 1, wants Seat 2. Tx B locks Seat 2, wants Seat 1.
    *   *Fix:* Always lock seats in ascending order of `seat_id`.
3.  **Q: Pessimistic vs Optimistic locking?**
    *   *A:* Pessimistic (`SELECT FOR UPDATE`) is good for high contention. Optimistic (version check) is good for low contention (avoids holding DB locks during user think time).

## Part 3: Tricky Fundamentals & C++ Features

### 13. Thread-Safe Singleton Initialization
**Problem:** Ensure a class is instantiated exactly once across multiple threads.
**Code:** `static Singleton& get() { static Singleton instance; return instance; }`
**Rigorous Follow-ups & Answers:**
1.  **Q: How is this thread-safe in C++11? What happens under the hood?**
    *   *A:* The compiler inserts a hidden flag and mutex (or atomic double-checked locking) around the static initialization. It guarantees the constructor runs exactly once, even if 100 threads call `get()` simultaneously.
2.  **Q: What if the Singleton constructor throws an exception?**
    *   *A:* The initialization is marked as failed. The next thread that calls `get()` will attempt to construct it again.
3.  **Q: Is using the Singleton afterwards (calling methods) thread-safe?**
    *   *A:* No. Initialization is thread-safe. Accessing its member variables still requires your own `std::mutex` if they are modified.

### 14. The "Thundering Herd" Problem
**Problem:** 100 threads wait on a `condition_variable`. `notify_all()` spikes CPU to 100%.
**Rigorous Follow-ups & Answers:**
1.  **Q: Why does the CPU spike?**
    *   *A:* All 100 threads wake up, fight for the single mutex, and 99 immediately fail the predicate and go back to sleep. This causes massive context switching and cache invalidation.
2.  **Q: How do you fix it?**
    *   *A:* Use `notify_one()` in a loop (if work is granular). Or use a LIFO queue instead of a FIFO queue to reduce contention. Or split the work into multiple queues.

### 15. Read-Write Locks (`std::shared_mutex`)
**Problem:** Allow multiple readers, but only one writer.
**Code:** `std::shared_mutex rw_lock; void read() { std::shared_lock lk(rw_lock); } void write() { std::unique_lock lk(rw_lock); }`
**Rigorous Follow-ups & Answers:**
1.  **Q: How can a writer starve in this setup?**
    *   *A:* If readers constantly arrive, the `shared_lock` is never fully released. The `unique_lock` (writer) waits forever.
2.  **Q: How to fix writer starvation?**
    *   *A:* Use a fair `std::shared_mutex` (if supported by OS) or implement a ticket system where writers get priority after N readers.
3.  **Q: What is the performance overhead of `std::shared_mutex` compared to `std::mutex`?**
    *   *A:* Much higher. It uses atomic operations to track reader count. If read operations are very fast (e.g., reading an int), a plain `std::mutex` is actually faster due to lower overhead.

### 16. Atomic Operations vs Mutexes
**Problem:** Why is `std::atomic<int> counter{0}; counter++;` faster than a mutex?
**Rigorous Follow-ups & Answers:**
1.  **Q: Why is it faster?**
    *   *A:* Atomics use CPU hardware instructions (like `LOCK XADD` on x86). They do not put the thread to sleep, require no OS context switch, and take ~20 clock cycles. A mutex requires a syscall and context switch (~1000+ cycles).
2.  **Q: When does an atomic become slower than a mutex?**
    *   *A:* Under extreme high contention. Multiple CPUs continuously writing to the same atomic cause "cache-line bouncing" (cache invalidation across cores). A mutex might sleep the thread, reducing bus traffic.
3.  **Q: What is `std::memory_order_relaxed`?**
    *   *A:* Guarantees atomicity of the operation, but provides no synchronization or ordering guarantees for *other* variables. Useful for counters where you don't care about strict memory visibility.

### 17. Lock-Free Queues (Memory Ordering)
**Problem:** SPSC queue using `std::atomic<int>` for head and tail.
**Rigorous Follow-ups & Answers:**
1.  **Q: Why do we need `std::memory_order_acquire` and `std::memory_order_release`?**
    *   *A:* `release` ensures all writes to the queue data happen *before* the tail pointer is updated. `acquire` ensures the consumer reads the tail pointer *before* reading the data. Without this, the consumer might see the new tail, but read stale/garbage data.
2.  **Q: Can we use `std::memory_order_relaxed`?**
    *   *A:* No. It would break the happens-before relationship. The data in the array might not be visible to the consumer core.
3.  **Q: Is this queue truly "wait-free"?**
    *   *A:* An SPSC ring buffer is wait-free. Operations complete in a bounded number of steps regardless of what the other thread is doing.

### 18. Thread-Local Storage (TLS)
**Problem:** Need a random number generator in a multithreaded program.
**Code:** `thread_local int thread_id = generate_id();`
**Rigorous Follow-ups & Answers:**
1.  **Q: Why is `std::rand()` bad?**
    *   *A:* It uses a hidden global state, causing data races if not mutex-protected. A mutex-protected RNG serializes all threads.
2.  **Q: Why is TLS better?**
    *   *A:* TLS gives every thread its own independent RNG state. No locks needed, 100% parallel.
3.  **Q: How is TLS implemented under the hood?**
    *   *A:* The OS allocates a segment of memory per thread (e.g., FS segment register on Linux). `thread_local` variables are accessed as offsets from that segment register.

### 19. False Sharing (Cache Lines)
**Problem:** `int arr[2];`. Thread A writes `arr[0]`, Thread B writes `arr[1]`. Incredibly slow.
**Rigorous Follow-ups & Answers:**
1.  **Q: Why is it slow?**
    *   *A:* **False Sharing.** `arr[0]` and `arr[1]` live on the same CPU cache line (usually 64 bytes). When Thread A writes, it invalidates the cache line on Thread B's core. Thread B has to fetch it from main memory, and vice versa.
2.  **Q: How do you fix it?**
    *   *A:* Pad the data so they sit on different cache lines. `struct PaddedInt { int val; char pad[60]; };` or use `alignas(64)`.
3.  **Q: How do you detect false sharing?**
    *   *A:* Use tools like `perf stat -e cache-misses` on Linux. If cache misses are astronomically high relative to instructions, it's a cache-line issue.

### 20. Deadlock Detection & Watchdogs
**Problem:** System deadlocks randomly. How to find it?
**Rigorous Follow-ups & Answers:**
1.  **Q: How do you find the deadlock without reading code?**
    *   *A:* Use `gdb` (attach to process, `thread apply all bt`), `strace`, or Valgrind/TSan. Look for threads stuck in `__lll_lock_wait`.
2.  **Q: How do you implement a runtime Watchdog?**
    *   *A:* A background thread that periodically checks if any thread has been holding a specific mutex for > 5 seconds. If so, it dumps the stack trace and aborts or restarts the process.
3.  **Q: What is the "Banker's Algorithm"?**
    *   *A:* A resource allocation and deadlock avoidance algorithm that checks if granting a resource will leave the system in a "safe state" where all processes can still finish.

### 21. Backpressure in Bounded Queues
**Problem:** Producer generates 10,000 events/sec. Consumer handles 1,000/sec. OOM.
**Rigorous Follow-ups & Answers:**
1.  **Q: What do you do?**
    *   *A:* Implement Backpressure. Use a bounded blocking queue. When full, the producer is blocked. This gives the consumer time to catch up.
2.  **Q: What if blocking the producer isn't allowed?**
    *   *A:* Use a Drop Policy (drop oldest, drop newest, or random drop) or load-shed (reject the request immediately with an error).
3.  **Q: How does TCP handle this?**
    *   *A:* TCP flow control uses a sliding window. The receiver tells the sender how much buffer space it has left. If 0, the sender stops.

### 22. C++20 `std::jthread` & Cooperative Cancellation
**Problem:** Stop a thread safely without using `pthread_kill`.
**Code:** `std::jthread t([](std::stop_token st) { while(!st.stop_requested()) { work(); }); }`
**Rigorous Follow-ups & Answers:**
1.  **Q: Why is `std::jthread` safer than `std::thread`?**
    *   *A:* It automatically calls `join()` in its destructor, preventing accidental `std::terminate()` crashes if the programmer forgets to join.
2.  **Q: Why is cooperative cancellation better than `pthread_kill`?**
    *   *A:* The thread can check the `stop_token` at safe points in its logic, ensuring it doesn't leave data in a corrupted state or mutexes locked when it exits.
3.  **Q: What if the thread is blocked on a `cv.wait()`? How does it get cancelled?**
    *   *A:* You must pass the `stop_token` to the CV. `cv.wait(lock, stop_token, predicate)`. The CV will wake up automatically when `stop_requested` is triggered.

## Part 4: Senior Parallel Algorithm Design

### 23. Multithreaded Argsort (100M elements, <1000 distinct)
**Problem:** Return sorted indices, stable, no locks, no copies.
**Solution Strategy:**
1. Phase 1: Each thread counts local frequencies.
2. Reduction: Sum global frequencies.
3. Prefix Sum: Calculate exact starting index in result array for each key.
4. Phase 2: Each thread iterates its chunk again. Calculates `target_pos = global_start[key] + local_offset[key]`. Writes index directly. **Lock-free!**
**Rigorous Follow-ups & Answers:**
1.  **Q: Why is Phase 2 lock-free?**
    *   *A:* Because `target_pos` mathematically guarantees no two threads will ever calculate the same position. Thread 1 might write to `result[500]` for key A, while Thread 2 writes to `result[800]` for key A. They don't interfere.
2.  **Q: How do you handle arbitrary keys (not just <1000)?**
    *   *A:* Replace `vector<int>` with `std::unordered_map<int64_t, int>` for local counts. The logic remains identical, but hash map overhead increases.
3.  **Q: Is this stable?**
    *   *A:* Yes. Because each thread processes its chunk sequentially (`start` to `end`), and the chunks are processed in order, the indices are inserted in their original relative order.

### 24. Lock-Free SPSC Ring Buffer
**Problem:** High-frequency trading queue. No `std::mutex`, no `new`.
**Code:**
```cpp
template <typename T>
class LockFreeSPSC {
    std::vector<T> buffer;
    const size_t capacity;
    alignas(64) std::atomic<size_t> head{0};
    alignas(64) std::atomic<size_t> tail{0};
public:
    LockFreeSPSC(size_t cap) : buffer(cap), capacity(cap) {}
    bool push(const T& item) {
        size_t current_head = head.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) % capacity;
        if (next_head == tail.load(std::memory_order_acquire)) return false;
        buffer[current_head] = item;
        head.store(next_head, std::memory_order_release);
        return true;
    }
    bool pop(T& item) {
        size_t current_tail = tail.load(std::memory_order_relaxed);
        if (current_tail == head.load(std::memory_order_acquire)) return false;
        item = buffer[current_tail];
        size_t next_tail = (current_tail + 1) % capacity;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }
};
```
**Rigorous Follow-ups & Answers:**
1.  **Q: Why `std::memory_order_acquire` and `release` instead of `seq_cst`?**
    *   *A:* `seq_cst` forces a global memory barrier across all CPUs, which is extremely slow. `acquire/release` only ensures a happens-before relationship between the producer and consumer for the specific `buffer` data. It allows the CPU to reorder other instructions for cache efficiency.
2.  **Q: Why `alignas(64)`?**
    *   *A:* To prevent **False Sharing**. `head` is written by the Producer, `tail` is written by the Consumer. If they share a 64-byte cache line, every write invalidates the cache on the other CPU core. Aligning them forces them onto separate cache lines.
3.  **Q: Is it possible for the Consumer to see the new `head` value but read stale `buffer` data?**
    *   *A:* No. `std::memory_order_release` on `head.store` guarantees that all writes to `buffer` before that line will be visible to any thread that does `std::memory_order_acquire` on `head.load`.

### 25. Parallel Prefix Sum (Scan)
**Problem:** Calculate running total of 100M numbers in parallel in O(log N).
**Solution Strategy (Blelloch Algorithm):**
1. Up-Sweep: Threads sum pairs, then pairs of pairs, building a tree.
2. Down-Sweep: Threads propagate the sums back down the tree.
**Rigorous Follow-ups & Answers:**
1.  **Q: Why not just use a single atomic counter?**
    *   *A:* An atomic counter serializes the additions. 100M threads/operations fighting for one cache line would take hours. The tree algorithm is O(N) work but O(log N) depth.
2.  **Q: How do you handle the tree without allocating new arrays?**
    *   *A:* Do it in-place. Use bitwise shifts to calculate indices (e.g., `stride = 1; stride <<= 1`).
3.  **Q: Is this actually faster than a single-threaded loop for 100M elements?**
    *   *A:* It depends on memory bandwidth. Single-threaded prefix sum is memory-bound. The parallel version uses more CPU instructions but spreads the memory accesses across cores. It usually wins at >10M elements.

### 26. Work-Stealing Thread Pool
**Problem:** Idle threads should steal tasks from busy threads' queues.
**Solution:** Each thread has its own `std::deque` (LIFO for owner, FIFO for stealers).
**Rigorous Follow-ups & Answers:**
1.  **Q: Why LIFO for the owner?**
    *   *A:* Cache locality. The task you just pushed is likely still in the L1 cache. Also, it creates depth-first execution, which is better for recursive divide-and-conquer algorithms.
2.  **Q: Why FIFO for the stealer?**
    *   *A:* The oldest tasks are at the bottom of the deque. They represent larger, independent chunks of work. Stealing them minimizes future stealing requests.
3.  **Q: How do you lock the deque?**
    *   *A:* The owner uses a lock-freedeque (e.g., Chase-Lev deque). The stealer uses a CAS operation. This minimizes contention.

### 27. Treiber Stack (Lock-Free Stack)
**Problem:** Lock-free LIFO stack.
**Code:** `head.compare_exchange_weak(old_head, new_head)`
**Rigorous Follow-ups & Answers:**
1.  **Q: Why `compare_exchange_weak` instead of `strong`?**
    *   *A:* `weak` can fail spuriously (return false even if values match). In a loop, this is fine and faster on ARM/POWER architectures where `strong` requires extra locking instructions.
2.  **Q: What is the ABA problem?**
    *   *A:* Thread A reads head=B. Thread B pops B, pops C, pushes B back. Thread A's CAS sees head=B and succeeds, but C is lost. 
    *   *Fix:* Use a versioned pointer (`std::atomic<std::pair<Node*, uint64_t>>` or hazard pointers).
3.  **Q: How do you safely free the node in `pop()`?**
    *   *A:* You can't immediately `delete` it. Another thread might be reading it. Use Hazard Pointers or Epoch-Based Reclamation (EBR).

### 28. Parallel Graph BFS
**Problem:** Traverse a graph in parallel.
**Solution:** Each level of BFS is a parallel loop. Threads process the frontier concurrently.
**Rigorous Follow-ups & Answers:**
1.  **Q: How do you avoid global synchronization between levels?**
    *   *A:* You can't. BFS inherently requires a barrier between levels. 
    *   *Optimization:* Use a lock-free queue for the next frontier. Each thread writes to its local queue, then merges them at the barrier.
2.  **Q: What is the performance bottleneck?**
    *   *A:* The frontier can grow exponentially (e.g., 1 -> 1000 -> 1000000). Memory bandwidth and cache contention on the visited array become the limit.
3.  **Q: How do you handle cycles?**
    *   *A:* A `std::atomic<bool>` array or a bitmap. `if (visited[i].test_and_set()) continue;`

### 29. Cache-Line Aware Matrix Transpose
**Problem:** Transpose 10,000x10,000 matrix.
**Solution:** Block the matrix into 64x64 tiles. Transpose tiles diagonally.
**Rigorous Follow-ups & Answers:**
1.  **Q: Why is naive transpose slow?**
    *   *A:* Reading row-wise is cache-friendly, but writing column-wise causes every write to miss the cache and evict a different line.
2.  **Q: Why 64x64 tiles?**
    *   *A:* A 64x64 tile of `float` is 16KB. It fits in L1 cache. You read the tile row-wise, and write the transposed tile row-wise, all inside the L1 cache.
3.  **Q: How do you parallelize it?**
    *   *A:* Assign tiles to threads. Diagonal tiles can be done in-place. Off-diagonal tiles require swapping data between two threads, needing careful synchronization or assigning pairs to the same thread.

### 30. Multithreaded Memory Pool
**Problem:** Lock-free for small allocs, mutex for large.
**Solution:** Maintain a free-list per size class. Small allocs pop from the lock-free free-list. Large allocs fall back to `malloc`.
**Rigorous Follow-ups & Answers:**
1.  **Q: How do you handle fragmentation?**
    *   *A:* Size classes. Round up every allocation to the nearest power of 2 (e.g., 8, 16, 32). This limits the number of free-lists and reduces fragmentation.
2.  **Q: How do you return memory to the OS?**
    *   *A:* Periodically, a background thread checks if a free-list is too large. It returns whole pages to the OS using `mmap` or `VirtualFree`.
3.  **Q: What is thread-local caching?**
    *   *A:* Each thread gets its own small free-list. It only hits the global lock-free list when its local list is empty or too full. This is how `jemalloc` and `tcmalloc` work.

### 31. Async Logger with Zero Allocation
**Problem:** High-throughput logger, never blocks main thread, never `new` during runtime.
**Solution:** Pre-allocate a massive ring buffer. Main thread formats string into buffer (using `snprintf`), sets an atomic flag. Background thread writes to disk.
**Rigorous Follow-ups & Answers:**
1.  **Q: What if the ring buffer is full?**
    *   *A:* Drop logs. In HFT, dropping logs is better than blocking the main thread. Increment a `dropped_count` atomic and log it later.
2.  **Q: Why not use `std::string`?**
    *   *A:* `std::string` dynamically allocates memory (`new`). Memory allocation requires locks inside the OS allocator. This causes unpredictable latency spikes.
3.  **Q: How do you handle variable-length strings?**
    *   *A:* Write a header struct `{uint16_t length; uint8_t level;}` followed by the raw chars. The background thread reads the length to know how much to write to disk.

### 32. Lock-Free Hash Map
**Problem:** Read-heavy, append-only.
**Solution:** Open addressing. Keys are atomic. Insert uses CAS. Reads are wait-free.
**Rigorous Follow-ups & Answers:**
1.  **Q: Why is deleting from a lock-free hash map hard?**
    *   *A:* If you delete a key, another thread might be mid-traversal of the open-addressing probe sequence. A tombstone (deleted marker) must be left to prevent the sequence from breaking.
2.  **Q: How do you resize?**
    *   *A:* You can't resize lock-free easily. You must allocate a new map, and gradually migrate items. Or, use an array of pointers to arrays (segmented array) to grow without moving existing data.
3.  **Q: What is Robin Hood Hashing?**
    *   *A:* On insert, if the new key is far from its ideal slot and the current slot is close to its ideal, the new key evicts the old one. This minimizes the maximum probe length, making reads very predictable.

### 33. Dining Philosophers with Asymmetric Resource Starvation
**Problem:** One philosopher is 10x hungrier.
**Solution:** The hungry philosopher uses a backoff timer. If he can't get both forks in 1ms, he drops them and waits, allowing neighbors to eat.
**Rigorous Follow-ups & Answers:**
1.  **Q: How does this prevent deadlock?**
    *   *A:* It breaks the Hold-and-Wait condition. The hungry philosopher never holds a fork while waiting for the second one for long.
2.  **Q: What is the downside of backoff?**
    *   *A:* Livelock. If all philosophers use the exact same backoff timer, they might synchronize their attempts and drops, never actually eating.
    *   *Fix:* Add random jitter to the backoff timer.
3.  **Q: What is the absolute optimal solution?**
    *   *A:* A central Concierge (Waiter). The hungry philosopher has a higher priority in the queue. The Concierge grants forks based on priority, completely eliminating deadlocks and starvation.
