#include <atomic>
#include <cstddef>
#include <functional>
#include <vector>

// A lock-free hash map, scoped deliberately: fixed bucket count (no
// resize), insert and find only (no delete). Each bucket is an intrusive,
// prepend-only, lock-free singly linked list, the same CAS-retry-loop
// shape as 015-treiber-stack. Because nodes are only ever prepended and
// never removed or freed once published, there is no reclamation problem
// to solve here at all -- see the reading for why "no delete" is the
// scope decision that makes that true, and what a real implementation
// needs once delete is required.
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
    // present (existing value left untouched).
    bool insert(const K& key, const V& value) {
        std::atomic<Node*>& head = buckets_[bucket_of(key)];
        Node* old_head = head.load(std::memory_order_acquire);
        Node* node = nullptr;
        for (;;) {
            // Re-scan under THIS attempt's snapshot of the head every time
            // through the loop -- not just once before the loop -- so a
            // concurrent insert of the same key that wins the race is
            // always visible before we'd otherwise duplicate it.
            for (Node* n = old_head; n; n = n->next) {
                if (n->key == key) {
                    delete node;  // no-op if we hadn't allocated yet
                    return false;
                }
            }
            if (!node) node = new Node{key, value, old_head};
            else node->next = old_head;

            if (head.compare_exchange_weak(old_head, node,
                                            std::memory_order_release,
                                            std::memory_order_acquire)) {
                return true;
            }
            // CAS failed: old_head was refreshed to the current value by
            // compare_exchange_weak itself. Loop and re-check with it.
        }
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
