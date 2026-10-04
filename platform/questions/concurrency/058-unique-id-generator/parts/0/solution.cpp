#include <atomic>
#include <cstdint>
#include <functional>
#include <utility>

class UniqueIdGenerator {
    static constexpr int kSeqBits = 12;
    static constexpr int kWorkerBits = 10;
    static constexpr std::uint64_t kSeqMask = (1ull << kSeqBits) - 1;   // 4095

    const std::uint64_t worker_id_;
    const std::function<std::uint64_t()> clock_;

    // Packed (tick << kSeqBits) | sequence. Both change together, atomically, via one
    // CAS -- that's what makes "is this a new tick" and "reset the sequence" a single
    // indivisible step instead of two separate operations with a gap between them.
    std::atomic<std::uint64_t> packed_{0};

public:
    UniqueIdGenerator(std::uint64_t worker_id, std::function<std::uint64_t()> clock)
        : worker_id_(worker_id), clock_(std::move(clock)) {}

    std::uint64_t next_id() {
        std::uint64_t old_state, new_state, tick, seq;
        do {
            old_state = packed_.load(std::memory_order_relaxed);
            const std::uint64_t old_tick = old_state >> kSeqBits;
            const std::uint64_t old_seq = old_state & kSeqMask;
            const std::uint64_t now = clock_();

            if (now > old_tick) {
                // The clock has genuinely moved forward: fresh tick, fresh sequence.
                tick = now;
                seq = 0;
            } else {
                // Same tick as last time (or the clock is behind our own advanced
                // notion of "current tick" -- see the overflow branch below).
                tick = old_tick;
                seq = old_seq + 1;
                if (seq > kSeqMask) {
                    // Out of sequence numbers for this tick before the clock caught
                    // up. Force our own tick forward rather than wrap and collide.
                    tick = old_tick + 1;
                    seq = 0;
                }
            }
            new_state = (tick << kSeqBits) | seq;
            // Every retry re-reads old_state fresh and recomputes tick/seq from THAT
            // read, so a losing CAS never commits decisions made against stale data.
        } while (!packed_.compare_exchange_weak(old_state, new_state,
                                                 std::memory_order_relaxed,
                                                 std::memory_order_relaxed));

        // worker_id_ never changes for this generator, so it needs no synchronization --
        // it's folded in only at the very end, after tick/seq are already settled.
        return (tick << (kSeqBits + kWorkerBits)) | (worker_id_ << kSeqBits) | seq;
    }
};
