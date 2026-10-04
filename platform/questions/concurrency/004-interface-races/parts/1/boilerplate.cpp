#include <memory>
#include <mutex>
#include <utility>
#include <vector>

// The interface race from part 0 is fixed: pop() is one operation under one
// lock. What is still wrong is what happens when T misbehaves.
template <typename T>
class threadsafe_stack {
    mutable std::mutex m_;
    std::vector<T> data_;

public:
    threadsafe_stack() = default;
    threadsafe_stack(const threadsafe_stack&) = delete;
    threadsafe_stack& operator=(const threadsafe_stack&) = delete;

    void push(T value) {
        std::lock_guard<std::mutex> g(m_);
        data_.push_back(std::move(value));
    }

    bool empty() const {
        std::lock_guard<std::mutex> g(m_);
        return data_.empty();
    }

    // TODO: grab it, remove it, wrap it. Read those three lines again and ask
    //       what state the world is in if line 3 throws.
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> g(m_);
        if (data_.empty()) return nullptr;
        T value = std::move(data_.back());
        data_.pop_back();
        return std::make_shared<T>(std::move(value));
    }

    // TODO: same shape, same problem. The element leaves the container before
    //       the caller has it.
    bool pop(T& out) {
        std::lock_guard<std::mutex> g(m_);
        if (data_.empty()) return false;
        T value = std::move(data_.back());
        data_.pop_back();
        out = std::move(value);
        return true;
    }
};
