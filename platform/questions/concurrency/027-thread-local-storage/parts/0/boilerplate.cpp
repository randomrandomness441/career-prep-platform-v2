#include <atomic>

// Every thread that calls station_id() gets its own station number,
// assigned once, the first time that thread calls it. Every thread's
// tickets are counted separately: next_ticket() increments and returns
// THIS thread's own counter, and issued_here() reports it. Two threads must
// never interfere with each other's station number or ticket count.
class ticket_station {
    // Per-process: one dispenser, atomic so concurrent first-calls are safe.
    inline static std::atomic<int> next_station_{0};

    // TODO: what state does each individual thread need of its own?

public:
    static int station_id() {
        // TODO: implement
        return -1;
    }

    static long next_ticket() {
        // TODO: implement
        return 0;
    }

    static long issued_here() {
        // TODO: implement
        return 0;
    }
};
