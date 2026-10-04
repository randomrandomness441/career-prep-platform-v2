#include <memory>
#include <mutex>
#include <utility>
#include <vector>

template <typename T>
class threadsafe_stack {
    mutable std::mutex m_;
    // The key move: the stack owns shared_ptrs, not Ts. The allocation and the
    // T construction both happen in push(), where a throw costs nothing,
    // because the element is not in the container yet.
    std::vector<std::shared_ptr<T>> data_;

public:
    threadsafe_stack() = default;
    threadsafe_stack(const threadsafe_stack&) = delete;
    threadsafe_stack& operator=(const threadsafe_stack&) = delete;

    void push(T value) {
        // Built before the lock is taken: if make_shared throws (bad_alloc, or
        // T's own constructor), nothing has been locked and nothing has been
        // modified. It also keeps the allocation out of the critical section.
        std::shared_ptr<T> p = std::make_shared<T>(std::move(value));
        std::lock_guard<std::mutex> g(m_);
        data_.push_back(std::move(p));
    }

    // Nothing here can throw. Moving a shared_ptr out of the vector is a
    // pointer swap, pop_back() on a vector of shared_ptrs is a destructor call
    // on a null pointer, and returning the shared_ptr is another move.
    // T's copy constructor is never invoked, so T cannot lose the element.
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> g(m_);
        if (data_.empty()) return nullptr;
        std::shared_ptr<T> result = std::move(data_.back());
        data_.pop_back();
        return result;
    }

    // The out-parameter form: the caller supplies the storage, so the only
    // risky step is the assignment into it — and that happens BEFORE the
    // element is removed. If the assignment throws, the element is untouched
    // and still on the stack, so the caller can try again.
    //
    // Deliberately a copy, not a move: a move-assignment that throws halfway
    // would leave the element on the stack in a moved-from state, which is a
    // quieter version of the same loss. The price is a real copy of T.
    bool pop(T& out) {
        std::lock_guard<std::mutex> g(m_);
        if (data_.empty()) return false;
        out = *data_.back();
        data_.pop_back();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> g(m_);
        return data_.empty();
    }
};
