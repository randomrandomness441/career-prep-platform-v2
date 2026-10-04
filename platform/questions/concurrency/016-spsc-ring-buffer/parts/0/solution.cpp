#include <array>
#include <atomic>
#include <cstddef>

// Single-producer / single-consumer ring buffer.
//
// Exactly one thread may call push(), exactly one other may call pop(). That
// restriction is what makes the whole thing work with plain loads and stores:
// head_ has one writer (the producer) and tail_ has one writer (the consumer),
// so neither index ever needs a read-modify-write.
template <typename T, std::size_t Capacity>
class SpscRing {
    static_assert(Capacity >= 2, "need at least two slots");
    // Power of two so that wrapping is `& kMask` instead of `% Capacity`.
    // An integer division is roughly 20 cycles on this core and a mask is one.
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    // One slot is always left empty so that "head == tail" means empty and
    // never means full.
    static constexpr std::size_t capacity() { return Capacity - 1; }

    // Producer only. False if the buffer is full. Never blocks, never allocates.
    bool push(const T& v) {
        // We are the only writer of head_, so nobody can have changed it since
        // we last stored it: relaxed is enough to read our own value.
        const std::size_t h = head_.load(std::memory_order_relaxed);
        const std::size_t next = (h + 1) & kMask;

        // acquire: pairs with the consumer's release store of tail_. We are
        // about to overwrite buf_[h], and the consumer read buf_[h] before it
        // published this tail_. This is what stops us from writing over a slot
        // the consumer has not finished reading.
        if (next == tail_.load(std::memory_order_acquire)) return false;

        buf_[h] = v;

        // release: everything written above — the element itself — becomes
        // visible to any thread that acquires this value of head_. This store
        // is the publication of the element. Relaxed here is the bug the naive
        // version has: the consumer would be allowed to see the new index and
        // the old bytes.
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer only. False if the buffer is empty. Never blocks.
    bool pop(T& out) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);  // our own index

        // acquire: pairs with the producer's release store of head_, so if we
        // see this index we also see the element written before it.
        if (t == head_.load(std::memory_order_acquire)) return false;

        out = buf_[t];

        // release: tells the producer the slot is free, and orders our read of
        // buf_[t] before that announcement, so the producer cannot overwrite a
        // slot we are still reading.
        tail_.store((t + 1) & kMask, std::memory_order_release);
        return true;
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    // head_ is written by the producer on every push, tail_ by the consumer on
    // every pop. Without alignas they land in the same 64-byte cache line, and
    // every push invalidates the consumer's copy of the line and vice versa —
    // false sharing. The two cores then trade the line back and forth for the
    // life of the program. See the measurement in the reading.
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) std::array<T, Capacity> buf_{};
};
