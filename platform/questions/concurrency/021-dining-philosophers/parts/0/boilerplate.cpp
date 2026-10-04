#include <functional>
#include <mutex>

// Five philosophers, five forks in a circle. Each one needs both forks next
// to them to eat, and must never hold a fork forever.
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

        // TODO: all five philosophers grab in the same rotational direction, so
        //       all five can end up holding one fork and waiting for the next.
        //       Find the condition that makes that cycle possible and remove it.
        forks_[left].lock();
        pickLeftFork();

        forks_[right].lock();
        pickRightFork();

        eat();

        putLeftFork();
        forks_[left].unlock();

        putRightFork();
        forks_[right].unlock();
    }
};
