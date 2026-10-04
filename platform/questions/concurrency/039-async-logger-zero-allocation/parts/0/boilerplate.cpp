#include <atomic>
#include <cstddef>
#include <cstring>
#include <vector>

// A fixed-capacity log buffer: many producer threads call log()
// concurrently, one consumer thread calls drain(). log() never allocates.
// This is a fill-once buffer, not a wraparound ring -- once capacity
// records have been claimed, log() returns false.
struct LogRecord {
    int producer_id = -1;
    long seq = -1;
    char message[48] = {};
};

class Logger {
public:
    explicit Logger(std::size_t capacity)
        : capacity_(capacity), records_(capacity), ready_(capacity) {}

    bool log(int producer_id, long seq, const char* msg) {
        // TODO: many producer threads call this concurrently. Does every
        //       call get a genuinely distinct slot?
        std::size_t slot = next_write_++;
        if (slot >= capacity_) return false;

        LogRecord& r = records_[slot];
        r.producer_id = producer_id;
        r.seq = seq;
        std::strncpy(r.message, msg, sizeof(r.message) - 1);

        ready_[slot].store(true, std::memory_order_release);
        return true;
    }

    std::vector<LogRecord> drain() {
        std::vector<LogRecord> out;
        std::size_t n = next_write_;
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
    std::size_t next_write_ = 0;
};
