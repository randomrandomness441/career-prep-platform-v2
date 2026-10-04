#include <array>
#include <atomic>
#include <cstddef>

// Single-producer / single-consumer ring buffer. Exactly one thread calls
// push(), exactly one other calls pop(), concurrently, no lock anywhere.
template <typename T, std::size_t Capacity>
class SpscRing {
    static_assert(Capacity >= 2, "need at least two slots");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    static constexpr std::size_t capacity() { return Capacity - 1; }

    // Producer only. False if the buffer is full. Never blocks, never allocates.
    bool push(const T& v) {
        // TODO: what memory order does each load/store below actually need?
        const std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t next = (h + 1) & kMask;
        if (next == tail_.load(std::memory_order_relaxed)) return false;
        buf_[h] = v;
        head_.store(next, std::memory_order_relaxed);
        return true;
    }

    // Consumer only. False if the buffer is empty. Never blocks.
    bool pop(T& out) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_relaxed)) return false;
        out = buf_[t];
        tail_.store((t + 1) & kMask, std::memory_order_relaxed);
        return true;
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    // TODO: head_ is written on every push, tail_ on every pop, by two
    //       different threads. What happens if they land on the same cache
    //       line, and what would you do about it?
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
    std::array<T, Capacity> buf_{};
};
