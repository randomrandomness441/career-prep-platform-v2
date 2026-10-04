#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

// A fixed-block memory pool: BlockCount blocks of BlockSize bytes each,
// carved out of one contiguous allocation made once at construction. After
// that, allocate()/deallocate() never call the global allocator again.
//
// The free list is intrusive (a block's own unused bytes store the "next
// free" index while it's on the list) and guarded by a plain mutex -- the
// free-list surgery itself is cheap, so there's no real throughput to buy by
// making it lock-free. See this question's reading for a real, measured
// account of a lock-free attempt that looks right, passes every correctness
// test, and still has a genuine data race ThreadSanitizer catches: recycling
// the same block for a "free-list link" one moment and live user payload the
// next is exactly the kind of memory reuse that needs hazard pointers or
// epoch-based reclamation to do safely lock-free, not just a generation-
// tagged CAS. That's real, substantial machinery this question doesn't ask
// you to build -- the mutex is the honest, clean answer here.
//
// The one bug this version fixes relative to the boilerplate: blocks_in_use()
// must be exact under concurrent load, so it's std::atomic, not a plain
// size_t incremented outside the lock.
template <std::size_t BlockSize, std::size_t BlockCount>
class MemoryPool {
    static_assert(BlockSize >= sizeof(std::uint32_t),
                  "a free block must have room to store the free-list link");

    static constexpr std::uint32_t kNull = 0xFFFFFFFFu;

public:
    MemoryPool() {
        for (std::uint32_t i = 0; i < BlockCount; ++i) {
            std::uint32_t next = (i + 1 == BlockCount) ? kNull : i + 1;
            std::memcpy(block_ptr(i), &next, sizeof(next));
            in_use_[i] = false;
        }
        free_top_ = (BlockCount == 0) ? kNull : 0u;
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    static constexpr std::size_t capacity() { return BlockCount; }
    static constexpr std::size_t block_size() { return BlockSize; }
    std::size_t blocks_in_use() const { return in_use_count_.load(std::memory_order_relaxed); }

    // Exhaustion policy: fail fast. Returns nullptr immediately when the pool
    // is empty -- no blocking, no growth, no fallback to the global heap.
    void* allocate() {
        std::uint32_t idx;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (free_top_ == kNull) return nullptr;  // exhausted
            idx = free_top_;
            std::memcpy(&free_top_, block_ptr(idx), sizeof(free_top_));
            in_use_[idx] = true;
        }
        in_use_count_.fetch_add(1, std::memory_order_relaxed);
        return block_ptr(idx);
    }

    // Returns false (and does nothing) for a pointer this pool did not just
    // hand out -- covers a double-free, or garbage.
    bool deallocate(void* p) {
        if (p == nullptr) return false;
        std::uint32_t idx = index_of_ptr(p);
        if (idx == kNull) return false;  // not a pointer this pool owns
        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (!in_use_[idx]) return false;  // double free -- refused, list untouched
            in_use_[idx] = false;
            std::memcpy(block_ptr(idx), &free_top_, sizeof(free_top_));
            free_top_ = idx;
        }
        in_use_count_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

private:
    void* block_ptr(std::uint32_t idx) { return storage_ + static_cast<std::size_t>(idx) * BlockSize; }
    const void* block_ptr(std::uint32_t idx) const { return storage_ + static_cast<std::size_t>(idx) * BlockSize; }

    std::uint32_t index_of_ptr(void* p) const {
        const auto* bytes = static_cast<const unsigned char*>(p);
        if (bytes < storage_) return kNull;
        std::size_t off = static_cast<std::size_t>(bytes - storage_);
        if (off % BlockSize != 0) return kNull;
        std::size_t idx = off / BlockSize;
        if (idx >= BlockCount) return kNull;
        return static_cast<std::uint32_t>(idx);
    }

    alignas(std::max_align_t) unsigned char storage_[BlockSize * BlockCount];
    bool in_use_[BlockCount == 0 ? 1 : BlockCount];
    std::uint32_t free_top_ = kNull;
    std::mutex mutex_;
    std::atomic<std::size_t> in_use_count_{0};
};
