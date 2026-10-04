#include <algorithm>
#include <mutex>
#include <vector>

// One mutex per grid cell. A robot moving from (r1,c1) to an adjacent
// (r2,c2) needs exclusive access to BOTH cells for the instant of the
// move -- otherwise two robots could believe they've both safely arrived
// at overlapping state. Two mutexes, acquired from two different robots'
// moves at the same time, in whatever order each robot happens to name
// its own current/target cell first, is exactly the two-mutex deadlock
// shape from 005-scoped-lock and 006-hierarchical-mutex -- here it's two
// robots trying to swap into each other's cell simultaneously.
class RobotCleaner {
public:
    RobotCleaner(int rows, int cols)
        : rows_(rows), cols_(cols), locks_(static_cast<std::size_t>(rows) * cols) {}

    void move(int r1, int c1, int r2, int c2) {
        std::mutex& a = cell(r1, c1);
        std::mutex& b = cell(r2, c2);
        // std::lock() acquires both without deadlocking against another
        // thread doing the same for the same two mutexes in the opposite
        // order -- it does not matter which order WE name them in here.
        std::lock(a, b);
        std::lock_guard<std::mutex> lk1(a, std::adopt_lock);
        std::lock_guard<std::mutex> lk2(b, std::adopt_lock);
        // ... perform the move / clean the target cell ...
    }

private:
    std::mutex& cell(int r, int c) {
        return locks_[static_cast<std::size_t>(r) * static_cast<std::size_t>(cols_) +
                       static_cast<std::size_t>(c)];
    }

    int rows_, cols_;
    std::vector<std::mutex> locks_;
};
