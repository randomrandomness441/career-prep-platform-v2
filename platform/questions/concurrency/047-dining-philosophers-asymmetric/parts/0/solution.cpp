#include <chrono>
#include <functional>
#include <mutex>
#include <random>
#include <thread>

// Same resource-hierarchy fork ordering as 021 (still what prevents
// deadlock) plus one addition: after eating, back off for a small,
// randomized amount of time before this call returns -- before the caller
// can possibly call wantsToEat() again. That's a deliberate cost imposed
// on every philosopher, including a caller that would otherwise hammer the
// table with zero delay between meals: it puts a floor under how much
// think-time everyone effectively gets, no matter how eager any one
// caller is.
//
// The jitter matters as much as the backoff: if every philosopher backed
// off by the exact same fixed amount, they could end up retrying in
// lockstep -- see the reading for what that failure mode looks like and
// why a fixed backoff alone isn't the fix.
class DiningPhilosophers {
    std::mutex forks_[5];

public:
    DiningPhilosophers() = default;

    void wantsToEat(int philosopher,
                    std::function<void()> pickLeftFork,
                    std::function<void()> pickRightFork,
                    std::function<void()> eat,
                    std::function<void()> putLeftFork,
                    std::function<void()> putRightFork) {
        const int left  = philosopher;
        const int right = (philosopher + 4) % 5;

        if (philosopher == 0) {
            forks_[right].lock();
            pickRightFork();
            forks_[left].lock();
            pickLeftFork();
        } else {
            forks_[left].lock();
            pickLeftFork();
            forks_[right].lock();
            pickRightFork();
        }

        eat();

        putLeftFork();
        forks_[left].unlock();

        putRightFork();
        forks_[right].unlock();

        thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> jitter_us(50, 150);
        std::this_thread::sleep_for(std::chrono::microseconds(jitter_us(rng)));
    }
};
