#include <atomic>
#include <cstddef>
#include <cstdint>

template <std::size_t N>
class CounterBank {
public:
    CounterBank() noexcept {
        for (std::size_t i = 0; i < N; ++i) slots_[i].v.store(0, std::memory_order_relaxed);
    }

    void increment(std::size_t i) noexcept {
        slots_[i].v.fetch_add(1, std::memory_order_relaxed);
    }

    long get(std::size_t i) const noexcept {
        return slots_[i].v.load(std::memory_order_relaxed);
    }

    std::uintptr_t address_of(std::size_t i) const noexcept {
        return reinterpret_cast<std::uintptr_t>(&slots_[i].v);
    }

private:
    // alignas(64) forces every Slot -- including each array element -- onto
    // its own 64-byte cache line. sizeof(Slot) is padded up to 64 to make
    // that true for the whole array, not just element 0.
    struct alignas(64) Slot {
        std::atomic<long> v{0};
    };
    Slot slots_[N];
};
