#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// A single append-only log, written concurrently by many threads, that
// must end up with records in a well-defined, stable order -- the order
// callers arrived, not the order the OS happens to schedule them.
//
// "The order callers arrived" is made precise with a ticket: each
// write_record() call gets a ticket via one atomic increment, and tickets
// are handed out in a real, total order (fetch_add is atomic -- two
// concurrent callers can never get the same ticket, and there is always a
// definite first and second). The log itself is then required to contain
// records in ticket order, regardless of which thread's write actually
// reaches the disk-stand-in first in real time.
class OrderedLog {
public:
    // Returns the ticket this call was assigned.
    std::uint64_t write_record(std::string record) {
        std::uint64_t ticket = next_ticket_.fetch_add(1, std::memory_order_relaxed);

        std::unique_lock<std::mutex> lk(m_);
        // Wait for every earlier ticket to have been written first. Only
        // the one thread whose ticket is currently up ever proceeds past
        // this wait; every other waiting thread's predicate is false.
        cv_.wait(lk, [&] { return ticket == next_to_write_; });

        entries_.push_back(std::move(record));
        ++next_to_write_;
        lk.unlock();
        cv_.notify_all();
        return ticket;
    }

    std::vector<std::string> contents() const {
        std::lock_guard<std::mutex> lk(m_);
        return entries_;
    }

private:
    std::atomic<std::uint64_t> next_ticket_{0};
    mutable std::mutex m_;
    std::condition_variable cv_;
    std::uint64_t next_to_write_ = 0;
    std::vector<std::string> entries_;
};
