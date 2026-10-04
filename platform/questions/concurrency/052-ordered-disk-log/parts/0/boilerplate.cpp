#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// A single append-only log, written concurrently by many threads, that
// must end up with records in the order callers arrived, not the order
// the OS happens to schedule them. Each call gets a ticket (returned to
// the caller); the log must contain records in ticket order.
class OrderedLog {
public:
    // Returns the ticket this call was assigned.
    std::uint64_t write_record(std::string record) {
        std::uint64_t ticket = next_ticket_.fetch_add(1, std::memory_order_relaxed);
        // TODO: implement
        (void)record;
        return ticket;
    }

    std::vector<std::string> contents() const {
        std::lock_guard<std::mutex> lk(m_);
        return entries_;
    }

private:
    std::atomic<std::uint64_t> next_ticket_{0};
    mutable std::mutex m_;
    std::vector<std::string> entries_;
};
