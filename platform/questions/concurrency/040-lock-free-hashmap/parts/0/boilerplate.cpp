#include <atomic>
#include <cstddef>
#include <functional>
#include <vector>

// A lock-free hash map: fixed bucket count (no resize), insert and find
// only (no delete). Each bucket is a singly linked list.
template <typename K, typename V>
class LockFreeHashMap {
public:
    explicit LockFreeHashMap(std::size_t num_buckets) : buckets_(num_buckets) {}

    ~LockFreeHashMap() {
        for (auto& head : buckets_) {
            Node* n = head.load(std::memory_order_relaxed);
            while (n) { Node* next = n->next; delete n; n = next; }
        }
    }

    LockFreeHashMap(const LockFreeHashMap&) = delete;
    LockFreeHashMap& operator=(const LockFreeHashMap&) = delete;

    // Returns true if `key` was newly inserted, false if it was already
    // present (existing value left untouched). Safe to call concurrently
    // from any number of threads, including with the same key.
    bool insert(const K& key, const V& value) {
        // TODO: implement
        (void)key;
        (void)value;
        return false;
    }

    bool find(const K& key, V* out) const {
        const std::atomic<Node*>& head = buckets_[bucket_of(key)];
        for (Node* n = head.load(std::memory_order_acquire); n; n = n->next) {
            if (n->key == key) {
                if (out) *out = n->value;
                return true;
            }
        }
        return false;
    }

private:
    struct Node {
        K key;
        V value;
        Node* next;
    };

    std::size_t bucket_of(const K& key) const {
        return std::hash<K>{}(key) % buckets_.size();
    }

    std::vector<std::atomic<Node*>> buckets_;
};
