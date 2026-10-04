#include <functional>
#include <mutex>

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
        const int left  = philosopher;             // fork i
        const int right = (philosopher + 4) % 5;   // fork i-1

        // Break the circular wait by making every philosopher acquire forks in
        // DESCENDING fork index.
        //
        //   philosopher 1..4 : left = i, right = i-1   -> i then i-1, already descending
        //   philosopher 0    : left = 0, right = 4     -> must take 4 first, not 0
        //
        // With one global order on the resources, a cycle is impossible: a cycle
        // needs somebody waiting "upwards", and nobody ever does.
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
