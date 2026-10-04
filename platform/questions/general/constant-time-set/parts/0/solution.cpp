#include <vector>

class FastSet {
    std::vector<int> dense_;    // elements currently in the set, packed at the front
    std::vector<int> sparse_;   // sparse_[x] = index of x inside dense_, if present
    int size_ = 0;

public:
    explicit FastSet(int n) : dense_(static_cast<std::size_t>(n)), sparse_(static_cast<std::size_t>(n)) {}

    // The double-check is what makes stale sparse_[x] data (leftover from before a
    // clear(), or simply never written) harmless instead of a false positive: it
    // must both point inside the CURRENT size, and the dense slot it points at must
    // actually hold x.
    bool contains(int x) const {
        const auto idx = static_cast<std::size_t>(sparse_[static_cast<std::size_t>(x)]);
        return idx < static_cast<std::size_t>(size_) && dense_[idx] == x;
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
        // Swap the last element into the vacated slot -- no shifting, O(1).
        dense_[static_cast<std::size_t>(idx)] = last;
        sparse_[static_cast<std::size_t>(last)] = idx;
        --size_;
    }

    // Nothing is erased -- clear() just declares the existing contents void by
    // resetting the one counter that says how much of dense_ is "real."
    void clear() { size_ = 0; }

    std::vector<int> to_vector() const {
        return std::vector<int>(dense_.begin(), dense_.begin() + size_);
    }
};
