#include <atomic>
#include <cstdint>
#include <functional>
#include <utility>

// Packs (tick, worker_id, sequence-within-this-tick) into one uint64_t.
// Safe to call concurrently from any number of threads on the same
// instance; never wraps the sequence back to 0 if a tick's 4096 slots run
// out mid-tick.
class UniqueIdGenerator {
    static constexpr int kSeqBits = 12;
    static constexpr int kWorkerBits = 10;
    static constexpr std::uint64_t kSeqMask = (1ull << kSeqBits) - 1;   // 4095

    const std::uint64_t worker_id_;
    const std::function<std::uint64_t()> clock_;

public:
    UniqueIdGenerator(std::uint64_t worker_id, std::function<std::uint64_t()> clock)
        : worker_id_(worker_id), clock_(std::move(clock)) {}

    std::uint64_t next_id() {
        // TODO: implement
        return 0;
    }
};
