#include <mutex>
#include <vector>

// One mutex per grid cell. A robot moving from (r1,c1) to an adjacent
// (r2,c2) needs exclusive access to BOTH cells for the instant of the move.
class RobotCleaner {
public:
    RobotCleaner(int rows, int cols)
        : rows_(rows), cols_(cols), locks_(static_cast<std::size_t>(rows) * cols) {}

    void move(int r1, int c1, int r2, int c2) {
        std::mutex& from = cell(r1, c1);
        std::mutex& to = cell(r2, c2);
        // TODO: two robots can call move() at the same time, each
        //       wanting the cell the other one currently holds. What
        //       happens then?
        std::lock_guard<std::mutex> lk1(from);
        std::lock_guard<std::mutex> lk2(to);
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
