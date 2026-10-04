// Harness for "Concurrent LLM Serving: Batching Dispatcher". Includes the
// candidate's file verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <future>
#include <thread>
#include <vector>

int main() {
    constexpr int kClients = 300;

    BatchDispatcher dispatcher([](const std::vector<Request>& reqs) {
        std::vector<Response> out;
        out.reserve(reqs.size());
        for (const auto& r : reqs) out.push_back(Response{r.value * 2});
        return out;
    });

    std::vector<std::future<Response>> futures(kClients);
    std::vector<std::thread> threads;
    for (int i = 0; i < kClients; ++i) {
        threads.emplace_back([&, i] {
            SHAKE();
            futures[static_cast<std::size_t>(i)] = dispatcher.submit(Request{i});
        });
    }
    for (auto& t : threads) t.join();

    std::size_t queued = dispatcher.pending_count();
    if (queued != static_cast<std::size_t>(kClients)) {
        std::printf("%zu requests queued after %d concurrent submit() calls, expected %d "
                    "-- some requests were lost (or the shared pending list was corrupted)\n",
                    queued, kClients, kClients);
        return 1;
    }

    dispatcher.flush();

    for (int i = 0; i < kClients; ++i) {
        Response r = futures[static_cast<std::size_t>(i)].get();
        if (r.value != i * 2) {
            std::printf("client %d: got response %d, expected %d -- results were mismatched "
                        "to the wrong caller\n", i, r.value, i * 2);
            return 1;
        }
    }

    std::printf("%d concurrent clients submitted, batched, and each received exactly its "
                "own correct response\n", kClients);
    return 0;
}
