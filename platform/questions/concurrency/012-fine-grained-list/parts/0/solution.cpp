#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

// Hand-over-hand ("lock coupling") locking. A concrete int list -- the
// technique doesn't depend on what type is stored, so there's no need to
// templatize it.
//
// The invariant every operation maintains: to look at or change a node's `next`
// pointer you must hold THAT node's mutex. Unlinking a node therefore needs two
// locks — the predecessor's (because you rewrite its `next`) and the victim's
// (because that is what stops anyone from being parked inside it).
//
// A walker holds at most two locks at a time and always takes them front-to-back,
// so no cycle can form and two walkers can sit at different points in the list.
class threadsafe_list {
    struct node {
        std::mutex m;
        std::optional<int> value;    // empty only for the dummy head
        std::unique_ptr<node> next;

        node() = default;
        explicit node(int v) : value(v) {}
    };

    // A dummy first node. It is never removed and never holds a value, which is
    // what removes the special cases: the "predecessor" of the first real
    // element is an ordinary node with an ordinary mutex.
    node head_;

public:
    threadsafe_list() = default;

    // Iterative, via remove_if — a chain of unique_ptrs destroyed by the default
    // destructor recurses once per element and blows the stack on a long list.
    ~threadsafe_list() { remove_if([](int) { return true; }); }

    threadsafe_list(const threadsafe_list&) = delete;
    threadsafe_list& operator=(const threadsafe_list&) = delete;

    void push_front(int value) {
        // Allocate and copy outside the lock: the only part that needs
        // protection is the two pointer writes.
        std::unique_ptr<node> fresh(new node(value));
        std::lock_guard<std::mutex> lk(head_.m);
        fresh->next = std::move(head_.next);
        head_.next = std::move(fresh);
    }

    void for_each(const std::function<void(int)>& f) {
        node* current = &head_;
        std::unique_lock<std::mutex> lk(head_.m);
        while (node* const next = current->next.get()) {
            // Take the next lock BEFORE dropping this one. That overlap is the
            // whole trick: while we hold `current`, nobody can unlink `next`,
            // so the pointer we are about to follow cannot go stale.
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            // f runs while `next` is locked, so the element cannot be destroyed
            // underneath the callback.
            f(*next->value);
            current = next;
            lk = std::move(next_lk);
        }
    }

    std::optional<int> find_first_if(const std::function<bool(int)>& pred) {
        node* current = &head_;
        std::unique_lock<std::mutex> lk(head_.m);
        while (node* const next = current->next.get()) {
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            if (pred(*next->value)) return *next->value;   // copied under the lock
            current = next;
            lk = std::move(next_lk);
        }
        return std::nullopt;
    }

    void remove_if(const std::function<bool(int)>& pred) {
        node* current = &head_;
        std::unique_lock<std::mutex> lk(head_.m);
        while (node* const next = current->next.get()) {
            std::unique_lock<std::mutex> next_lk(next->m);
            if (pred(*next->value)) {
                // Both locks are held. Anyone who wants to reach `next` must
                // first hold `current`, and we have it — so no walker is inside
                // `next` and none can enter.
                std::unique_ptr<node> doomed = std::move(current->next);
                current->next = std::move(next->next);
                next_lk.unlock();
                // `doomed` is destroyed here, with its own mutex released first
                // (destroying a locked mutex is undefined) and while we still
                // hold `current`, which is what makes it unreachable.
            } else {
                lk.unlock();
                current = next;
                lk = std::move(next_lk);
            }
        }
    }
};
