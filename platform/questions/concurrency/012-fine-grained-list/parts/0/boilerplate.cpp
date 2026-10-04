#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

// A linked list, safe for any number of threads to push, walk, search, and
// remove from at once, without all of them serializing on one lock. Two
// threads working on different parts of the list should be able to run at
// the same time.
class threadsafe_list {
    struct node {
        std::mutex m;
        std::optional<int> value;    // empty only for the dummy head
        std::unique_ptr<node> next;

        node() = default;
        explicit node(int v) : value(v) {}
    };

    node head_;   // dummy first node

public:
    threadsafe_list() = default;

    threadsafe_list(const threadsafe_list&) = delete;
    threadsafe_list& operator=(const threadsafe_list&) = delete;

    void push_front(int value) {
        // TODO: implement
        (void)value;
    }

    void for_each(const std::function<void(int)>& f) {
        // TODO: implement
        (void)f;
    }

    std::optional<int> find_first_if(const std::function<bool(int)>& pred) {
        // TODO: implement
        (void)pred;
        return std::nullopt;
    }

    void remove_if(const std::function<bool(int)>& pred) {
        // TODO: implement
        (void)pred;
    }
};
