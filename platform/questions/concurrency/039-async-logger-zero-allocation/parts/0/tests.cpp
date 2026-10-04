// Harness for "Async Logger, Zero Allocation". Includes the candidate's
// file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <cstring>
#include <set>
#include <thread>
#include <vector>

int main() {
    // ── 1. many producers, one drain, no corruption or loss ──────────────
    {
        constexpr int kProducers = 16;
        constexpr int kPerProducer = 3000;
        Logger logger(static_cast<std::size_t>(kProducers) * kPerProducer);

        std::vector<std::thread> threads;
        for (int p = 0; p < kProducers; ++p) {
            threads.emplace_back([&, p] {
                for (int s = 0; s < kPerProducer; ++s) {
                    char msg[48];
                    std::snprintf(msg, sizeof(msg), "p%d-s%d", p, s);
                    bool ok = logger.log(p, s, msg);
                    if ((s & 511) == 0) SHAKE();
                    if (!ok) {
                        std::printf("log() returned false with a buffer sized for every "
                                    "message -- capacity was miscounted\n");
                        std::exit(1);
                    }
                }
            });
        }
        for (auto& t : threads) t.join();

        std::vector<LogRecord> out = logger.drain();
        if (out.size() != static_cast<std::size_t>(kProducers) * kPerProducer) {
            std::printf("drain() returned %zu records, expected %d -- some messages were "
                        "lost (overwritten by another producer's claim on the same slot)\n",
                        out.size(), kProducers * kPerProducer);
            return 1;
        }

        std::set<long long> seen;
        for (const auto& r : out) {
            char expected[48];
            std::snprintf(expected, sizeof(expected), "p%d-s%ld", r.producer_id, r.seq);
            if (std::strncmp(r.message, expected, sizeof(expected)) != 0) {
                std::printf("record (producer %d, seq %ld) has message \"%s\", expected "
                            "\"%s\" -- corrupted, likely two producers wrote the same slot\n",
                            r.producer_id, r.seq, r.message, expected);
                return 1;
            }
            long long key = static_cast<long long>(r.producer_id) * 1000000LL + r.seq;
            if (!seen.insert(key).second) {
                std::printf("(producer %d, seq %ld) appeared twice in drain() -- two "
                            "producer calls claimed the same slot\n", r.producer_id, r.seq);
                return 1;
            }
        }
    }

    // ── 2. a full buffer refuses new entries instead of corrupting ───────
    {
        Logger logger(4);
        int accepted = 0;
        for (int i = 0; i < 10; ++i) {
            if (logger.log(0, i, "x")) ++accepted;
        }
        if (accepted != 4) {
            std::printf("a 4-slot logger accepted %d of 10 log() calls, expected exactly 4\n",
                        accepted);
            return 1;
        }
        if (logger.drain().size() != 4) {
            std::printf("drain() after filling a 4-slot logger returned %zu records, "
                        "expected 4\n", logger.drain().size());
            return 1;
        }
    }

    std::printf("16 producers x 3000 messages each: no loss, no corruption, no duplicate "
                "slots; a full buffer correctly refuses new entries\n");
    return 0;
}
