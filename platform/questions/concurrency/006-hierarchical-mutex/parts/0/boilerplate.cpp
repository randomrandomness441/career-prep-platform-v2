#include <climits>
#include <mutex>
#include <stdexcept>

// A mutex that refuses to be locked out of order: acquiring it while already
// holding a mutex at the same level or a lower one throws instead of letting
// a genuine hierarchy violation slip through.
class hierarchical_mutex {
    std::mutex internal_mutex;
    unsigned long const hierarchy_value;

    // TODO: a thread needs to remember the level of the lowest-level mutex it
    //       currently holds. It has to be per-thread -- two threads walking
    //       the hierarchy at the same time must not see each other's number.
    //       Start it at ULONG_MAX, meaning "this thread holds nothing yet".

    // TODO: throw std::logic_error unless hierarchy_value is strictly less
    //       than the thread's current level.
    void check_for_hierarchy_violation() const {
        (void)hierarchy_value;
    }

public:
    explicit hierarchical_mutex(unsigned long value)
        : hierarchy_value(value) {}

    hierarchical_mutex(const hierarchical_mutex&) = delete;
    hierarchical_mutex& operator=(const hierarchical_mutex&) = delete;

    void lock() {
        // Check first, block second: checking after you already own the
        // mutex is too late, because by then you may be the deadlock you
        // meant to report.
        check_for_hierarchy_violation();
        internal_mutex.lock();
        // TODO: on success, save the thread's previous level somewhere and
        //       set the thread's current level to this mutex's level.
    }

    void unlock() {
        // TODO: restore the level that was current before this lock() call.
        internal_mutex.unlock();
    }

    bool try_lock() {
        // TODO: same rules as lock(), except a failed try_lock must leave
        //       everything unchanged.
        check_for_hierarchy_violation();
        return internal_mutex.try_lock();
    }
};
