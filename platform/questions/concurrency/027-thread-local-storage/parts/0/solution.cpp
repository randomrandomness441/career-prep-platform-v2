#include <atomic>

class ticket_station {
    // Per-process: one dispenser, atomic so concurrent first-calls are safe.
    inline static std::atomic<int> next_station_{0};

    // Per-thread: each thread gets its own copy, created on first use,
    // destroyed when the thread exits. No locks, no contention — the object
    // is reachable only from the thread that owns it.
    inline static thread_local int station_ = -1;
    inline static thread_local long tickets_ = 0;

public:
    static int station_id() {
        // Safe without a lock because station_ belongs to this thread: each
        // thread runs this check at most once.
        if (station_ < 0)
            station_ = next_station_.fetch_add(1, std::memory_order_relaxed);
        return station_;
    }

    static long next_ticket() { return ++tickets_; }

    static long issued_here() { return tickets_; }
};
