#include "solution.hpp"

#include "concur/stress.hpp"
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// value_for must hand back a COPY. A reference would point into a bucket that
// another thread is free to erase the instant the lock is released.
using table_ii = threadsafe_lookup_table<int, int>;
static_assert(std::is_same<decltype(std::declval<const table_ii&>().value_for(1, 0)), int>::value,
              "value_for must return Value by value, not a reference");

namespace {

int single_threaded(unsigned num_buckets) {
    threadsafe_lookup_table<int, int> t(num_buckets);

    if (t.value_for(42, -1) != -1) {
        std::printf("[%u buckets] missing key: value_for(42, -1) returned %d, expected -1\n",
                    num_buckets, t.value_for(42, -1));
        return 1;
    }
    if (t.value_for(42) != 0) {
        std::printf("[%u buckets] missing key with no default should give Value(): got %d\n",
                    num_buckets, t.value_for(42));
        return 1;
    }

    t.add_or_update_mapping(42, 7);
    if (t.value_for(42, -1) != 7) {
        std::printf("[%u buckets] after add: got %d, expected 7\n",
                    num_buckets, t.value_for(42, -1));
        return 1;
    }

    t.add_or_update_mapping(42, 9);           // update, not a second entry
    if (t.value_for(42, -1) != 9) {
        std::printf("[%u buckets] after update: got %d, expected 9 "
                    "(a second entry for the same key shadows the new value)\n",
                    num_buckets, t.value_for(42, -1));
        return 1;
    }

    t.remove_mapping(42);
    if (t.value_for(42, -1) != -1) {
        std::printf("[%u buckets] after remove: got %d, expected -1\n",
                    num_buckets, t.value_for(42, -1));
        return 1;
    }
    t.remove_mapping(42);                     // removing twice must be harmless

    for (int i = 0; i < 500; ++i) t.add_or_update_mapping(i, i * 3);
    for (int i = 0; i < 500; i += 2) t.remove_mapping(i);
    for (int i = 0; i < 500; ++i) {
        const int want = (i % 2 == 0) ? -1 : i * 3;
        if (t.value_for(i, -1) != want) {
            std::printf("[%u buckets] key %d: got %d, expected %d\n",
                        num_buckets, i, t.value_for(i, -1), want);
            return 1;
        }
    }
    return 0;
}

}  // namespace

int main() {
    // ---- 1. plain behaviour, with a normal table and with one where every key
    //         collides into the same bucket ------------------------------------
    for (unsigned nb : {19u, 1u, 257u}) {
        if (single_threaded(nb) != 0) return 1;
    }

    // ---- 2. string keys ----------------------------------------------------
    {
        threadsafe_lookup_table<std::string, std::string> t(11);
        t.add_or_update_mapping("host", "localhost");
        t.add_or_update_mapping("host", "example.com");
        if (t.value_for("host", "?") != "example.com" ||
            t.value_for("port", "8777") != "8777") {
            std::printf("string table: got host=%s port=%s\n",
                        t.value_for("host", "?").c_str(),
                        t.value_for("port", "8777").c_str());
            return 1;
        }
    }

    // ---- 3. readers and writers together -----------------------------------
    const int readers = 6, writers = 4, per_writer = 400;
    for (int trial = 0; trial < 4; ++trial) {
        threadsafe_lookup_table<int, long long> t(23);

        // Keys 0..399 are set once and never touched again: any reader that sees
        // anything other than key*2 has read torn or stale state.
        for (int i = 0; i < 400; ++i) t.add_or_update_mapping(i, i * 2);

        std::atomic<int> bad_reads{0};
        std::atomic<long long> observed{0};
        std::vector<std::thread> threads;

        for (int r = 0; r < readers; ++r) {
            threads.emplace_back([&t, &bad_reads, &observed, r] {
                long long seen = 0;
                for (int i = 0; i < 4000; ++i) {
                    const int key = (i * 7 + r * 13) % 400;
                    const long long got = t.value_for(key, -1);
                    if (got != key * 2) ++bad_reads;
                    seen += got;
                    if ((i & 511) == 0) SHAKE();
                }
                observed += seen;
            });
        }

        // Each writer owns a disjoint key range, so the final contents are known.
        for (int w = 0; w < writers; ++w) {
            threads.emplace_back([&t, w, per_writer] {
                const int base = 10000 + w * 1000;
                for (int i = 0; i < per_writer; ++i) {
                    t.add_or_update_mapping(base + i, 1);
                    SHAKE();
                    t.add_or_update_mapping(base + i, base + i);
                    if (i % 3 == 0) t.remove_mapping(base + i);
                }
            });
        }

        for (auto& th : threads) th.join();

        if (bad_reads.load() != 0) {
            std::printf("%d reads of an unchanging key came back wrong while writers "
                        "were active\n", bad_reads.load());
            return 1;
        }
        for (int w = 0; w < writers; ++w) {
            const int base = 10000 + w * 1000;
            for (int i = 0; i < per_writer; ++i) {
                const long long want = (i % 3 == 0) ? -1 : base + i;
                const long long got = t.value_for(base + i, -1);
                if (got != want) {
                    std::printf("writer %d key %d: got %lld, expected %lld\n",
                                w, base + i, got, want);
                    return 1;
                }
            }
        }
        for (int i = 0; i < 400; ++i) {
            if (t.value_for(i, -1) != i * 2) {
                std::printf("untouched key %d was corrupted: %lld\n",
                            i, t.value_for(i, -1));
                return 1;
            }
        }
    }

    // ---- 4. many threads hammering ONE bucket ------------------------------
    {
        threadsafe_lookup_table<int, int> t(1);   // everything collides
        std::atomic<int> bad{0};
        std::vector<std::thread> threads;
        for (int i = 0; i < 200; ++i) t.add_or_update_mapping(i, i);
        for (int th = 0; th < 8; ++th) {
            threads.emplace_back([&t, &bad, th] {
                for (int i = 0; i < 2000; ++i) {
                    if (th < 6) {
                        if (t.value_for(i % 200, -1) != i % 200) ++bad;
                    } else {
                        t.add_or_update_mapping(1000 + th, i);
                        SHAKE();
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
        if (bad.load() != 0) {
            std::printf("%d bad reads with all keys in one bucket\n", bad.load());
            return 1;
        }
    }

    std::printf("lookup table: correct under readers and writers, single bucket and many\n");
    return 0;
}
