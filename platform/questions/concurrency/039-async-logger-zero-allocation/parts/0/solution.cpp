#include <atomic>
#include <cstddef>
#include <cstring>
#include <vector>

// A fixed-capacity log buffer: many producer threads call log() concurrently,
// one consumer thread calls drain(). Every record is a fixed-size struct
// inside a buffer sized once at construction -- log() never allocates, so it
// never contends on the global allocator's own lock, and never has
// unpredictable allocation latency on a caller that's trying to be fast.
//
// This is a fill-once buffer, not a wraparound ring: once capacity records
// have been claimed, log() returns false and the caller decides what to do
// (drop, block, escalate). See the reading for why a real wraparound ring
// buffer needs more machinery than this.
struct LogRecord {
    int producer_id = -1;
    long seq = -1;
    char message[48] = {};
};

class Logger {
public:
    explicit Logger(std::size_t capacity)
        : capacity_(capacity), records_(capacity), ready_(capacity) {}

    // Producers: many threads call this concurrently. Never allocates.
    bool log(int producer_id, long seq, const char* msg) {
        // Every call claims a distinct slot via one atomic increment -- no
        // two producers can ever be given the same index, so the write into
        // records_[slot] below touches memory no other thread is touching.
        std::size_t slot = next_write_.fetch_add(1, std::memory_order_relaxed);
        if (slot >= capacity_) return false;  // full; caller's choice what to do

        LogRecord& r = records_[slot];
        r.producer_id = producer_id;
        r.seq = seq;
        std::strncpy(r.message, msg, sizeof(r.message) - 1);

        // Published last, with release: the consumer's acquire load of this
        // same flag (in drain()) makes everything written above visible to
        // it, in the right order, before it reads the record.
        ready_[slot].store(true, std::memory_order_release);
        return true;
    }

    // Consumer: exactly one thread calls this. Returns every record
    // currently marked ready, in slot order.
    std::vector<LogRecord> drain() {
        std::vector<LogRecord> out;
        std::size_t n = next_write_.load(std::memory_order_relaxed);
        if (n > capacity_) n = capacity_;
        for (std::size_t i = 0; i < n; ++i) {
            if (ready_[i].load(std::memory_order_acquire)) out.push_back(records_[i]);
        }
        return out;
    }

    std::size_t capacity() const { return capacity_; }

private:
    std::size_t capacity_;
    std::vector<LogRecord> records_;
    std::vector<std::atomic<bool>> ready_;
    std::atomic<std::size_t> next_write_{0};
};
