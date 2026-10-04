#include <cstddef>
#include <random>
#include <vector>

class Skiplist {
    static constexpr int kMaxLevel = 16;

    struct Node {
        int val;
        std::vector<Node*> forward;
        Node(int v, int levels) : val(v), forward(static_cast<std::size_t>(levels), nullptr) {}
    };

    Node* head_;
    int level_ = 1;   // number of levels currently in use
    std::mt19937 rng_;

    int random_level() {
        std::uniform_real_distribution<double> coin(0.0, 1.0);
        int lvl = 1;
        while (lvl < kMaxLevel && coin(rng_) < 0.5) ++lvl;
        return lvl;
    }

public:
    Skiplist() : head_(new Node(-1, kMaxLevel)), rng_(12345) {}

    ~Skiplist() {
        Node* cur = head_;
        while (cur) {
            Node* nxt = cur->forward[0];
            delete cur;
            cur = nxt;
        }
    }

    Skiplist(const Skiplist&) = delete;
    Skiplist& operator=(const Skiplist&) = delete;

    bool search(int target) const {
        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            const auto lvl = static_cast<std::size_t>(i);
            while (cur->forward[lvl] && cur->forward[lvl]->val < target) cur = cur->forward[lvl];
        }
        cur = cur->forward[0];
        return cur != nullptr && cur->val == target;
    }

    void add(int num) {
        std::vector<Node*> update(static_cast<std::size_t>(kMaxLevel), head_);
        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            const auto lvl = static_cast<std::size_t>(i);
            while (cur->forward[lvl] && cur->forward[lvl]->val < num) cur = cur->forward[lvl];
            update[lvl] = cur;
        }

        const int lvl = random_level();
        if (lvl > level_) {
            for (int i = level_; i < lvl; ++i) update[static_cast<std::size_t>(i)] = head_;
            level_ = lvl;
        }

        Node* fresh = new Node(num, kMaxLevel);
        for (int i = 0; i < lvl; ++i) {
            const auto u = static_cast<std::size_t>(i);
            fresh->forward[u] = update[u]->forward[u];
            update[u]->forward[u] = fresh;
        }
    }

    // Removes one occurrence of num, unlinking it at EVERY level it participates in --
    // not just level 0. A node left dangling at a higher level is still reachable by
    // any future descent that passes through it, pointing at freed memory.
    bool erase(int num) {
        std::vector<Node*> update(static_cast<std::size_t>(kMaxLevel), head_);
        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            const auto lvl = static_cast<std::size_t>(i);
            while (cur->forward[lvl] && cur->forward[lvl]->val < num) cur = cur->forward[lvl];
            update[lvl] = cur;
        }
        cur = cur->forward[0];
        if (!cur || cur->val != num) return false;

        for (int i = 0; i < level_; ++i) {
            const auto lvl = static_cast<std::size_t>(i);
            if (update[lvl]->forward[lvl] != cur) break;   // cur doesn't exist at this level
            update[lvl]->forward[lvl] = cur->forward[lvl];
        }
        delete cur;
        return true;
    }
};
