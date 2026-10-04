#include <vector>

// A set of integers in [0, n), with insert/remove/contains/clear all O(1).
class FastSet {
    std::vector<int> dense_;
    std::vector<int> sparse_;
    int size_ = 0;

public:
    explicit FastSet(int n) : dense_(static_cast<std::size_t>(n)), sparse_(static_cast<std::size_t>(n)) {}

    bool contains(int x) const {
        // TODO: implement
        (void)x;
        return false;
    }

    void insert(int x) {
        if (contains(x)) return;
        dense_[static_cast<std::size_t>(size_)] = x;
        sparse_[static_cast<std::size_t>(x)] = size_;
        ++size_;
    }

    void remove(int x) {
        if (!contains(x)) return;
        const int idx = sparse_[static_cast<std::size_t>(x)];
        const int last = dense_[static_cast<std::size_t>(size_ - 1)];
        dense_[static_cast<std::size_t>(idx)] = last;
        sparse_[static_cast<std::size_t>(last)] = idx;
        --size_;
    }

    void clear() { size_ = 0; }

    std::vector<int> to_vector() const {
        return std::vector<int>(dense_.begin(), dense_.begin() + size_);
    }
};
