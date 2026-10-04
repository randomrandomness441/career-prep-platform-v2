#include <mutex>
#include <optional>
#include <utility>
#include <vector>

// A stack, safe for any number of threads to push onto and pop from at once.
class threadsafe_stack {
    mutable std::mutex m_;
    std::vector<int> data_;

public:
    threadsafe_stack() = default;
    threadsafe_stack(const threadsafe_stack&) = delete;
    threadsafe_stack& operator=(const threadsafe_stack&) = delete;

    void push(int value) {
        std::lock_guard<std::mutex> g(m_);
        data_.push_back(value);
    }

    // TODO: give the caller the top value and remove it, as one operation a
    //       caller can't split apart. If two threads both want "the top
    //       value", each one that succeeds must get a different value, and
    //       neither should ever see a value the other one already took.
    //       What should this return when the stack is empty?
    std::optional<int> pop() {
        // TODO: implement
        return std::nullopt;
    }

    bool empty() const {
        std::lock_guard<std::mutex> g(m_);
        return data_.empty();
    }
};
