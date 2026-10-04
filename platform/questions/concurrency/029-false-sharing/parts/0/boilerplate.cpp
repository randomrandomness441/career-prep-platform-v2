#include <atomic>
#include <cstddef>
#include <cstdint>

// N independent counters. Different threads increment different counters
// concurrently -- no two threads ever touch the same one.
template <std::size_t N>
class CounterBank {
public:
    CounterBank() noexcept {
        for (std::size_t i = 0; i < N; ++i) slots_[i].store(0, std::memory_order_relaxed);
    }

    void increment(std::size_t i) noexcept {
        slots_[i].fetch_add(1, std::memory_order_relaxed);
    }

    long get(std::size_t i) const noexcept {
        return slots_[i].load(std::memory_order_relaxed);
    }

    // TODO: N counters, packed into a plain array, means 8 of them (on a
    // 64-bit long) fit in a single 64-byte cache line. Two threads touching
    // two different counters end up fighting over the same line.
    std::uintptr_t address_of(std::size_t i) const noexcept {
        return reinterpret_cast<std::uintptr_t>(&slots_[i]);
    }

private:
    std::atomic<long> slots_[N];
};
