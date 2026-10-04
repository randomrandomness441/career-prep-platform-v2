#include <atomic>
#include <thread>
#include <vector>

// A counter meant to be incremented extremely often, from many threads --
// think "count every request/event in a stream" -- and read back
// occasionally. Sharded: each thread increments a shard picked from its
// own thread id (no cross-thread contention on the hot path at all, since
// different threads land on different shards almost always), and total()
// sums every shard, which only has to happen on the rare read.
class StreamCounter {
public:
    explicit StreamCounter(int num_shards) : shards_(static_cast<std::size_t>(num_shards)) {
        for (auto& s : shards_) s.store(0, std::memory_order_relaxed);
    }

    void increment() {
        std::size_t shard = std::hash<std::thread::id>{}(std::this_thread::get_id()) % shards_.size();
        shards_[shard].fetch_add(1, std::memory_order_relaxed);
    }

    long total() const {
        long sum = 0;
        for (auto& s : shards_) sum += s.load(std::memory_order_relaxed);
        return sum;
    }

private:
    std::vector<std::atomic<long>> shards_;
};
