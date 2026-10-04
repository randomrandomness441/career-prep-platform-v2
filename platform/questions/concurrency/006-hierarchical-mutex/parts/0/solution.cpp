#include <climits>
#include <mutex>
#include <stdexcept>

// A mutex that refuses to be locked out of order.
//
// The whole design rests on one number per thread: the level of the lowest-level
// mutex that thread currently holds. It lives in thread-local storage, so every
// thread has its own copy and no synchronisation is needed to read or write it.
class hierarchical_mutex {
    std::mutex internal_mutex;
    unsigned long const hierarchy_value;
    unsigned long previous_hierarchy_value;

    // ULONG_MAX means "holding nothing", so the first lock of any level passes.
    inline static thread_local unsigned long this_thread_hierarchy_value = ULONG_MAX;

    void check_for_hierarchy_violation() const {
        // Strictly less: locking two mutexes of the *same* level is also a
        // violation, because nothing then fixes the order between them.
        if (this_thread_hierarchy_value <= hierarchy_value)
            throw std::logic_error("mutex hierarchy violated");
    }

    void update_hierarchy_value() {
        // Saved in the mutex, not the thread, because unlocks nest: each mutex
        // remembers the level that was current when *it* was locked.
        previous_hierarchy_value = this_thread_hierarchy_value;
        this_thread_hierarchy_value = hierarchy_value;
    }

public:
    explicit hierarchical_mutex(unsigned long value)
        : hierarchy_value(value), previous_hierarchy_value(0) {}

    hierarchical_mutex(const hierarchical_mutex&) = delete;
    hierarchical_mutex& operator=(const hierarchical_mutex&) = delete;

    void lock() {
        // Check BEFORE blocking. If we waited until we owned the mutex we would
        // already be deadlocked in the very case we are trying to report.
        check_for_hierarchy_violation();
        internal_mutex.lock();
        update_hierarchy_value();
    }

    void unlock() {
        this_thread_hierarchy_value = previous_hierarchy_value;
        internal_mutex.unlock();
    }

    bool try_lock() {
        check_for_hierarchy_violation();
        if (!internal_mutex.try_lock()) return false;
        update_hierarchy_value();
        return true;
    }
};
