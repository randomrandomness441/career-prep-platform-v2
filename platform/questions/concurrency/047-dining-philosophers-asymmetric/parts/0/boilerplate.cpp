#include <functional>
#include <mutex>

// Same five philosophers, five forks as [[021-dining-philosophers]],
// deadlock-free the same way. This question asks about something
// deadlock-freedom alone doesn't guarantee: fairness.
class DiningPhilosophers {
    std::mutex forks_[5];

public:
    DiningPhilosophers() = default;

    // TODO: what stops a philosopher who calls wantsToEat() again the
    //       instant their previous meal ends from simply out-competing a
    //       neighbour for the shared forks, over and over?
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
    }
};
