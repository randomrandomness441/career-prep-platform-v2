// Harness for Print Zero Even Odd. Includes the candidate's class verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static std::string render(const std::vector<int>& v, std::size_t from, std::size_t count) {
    std::string s;
    for (std::size_t i = from; i < v.size() && i < from + count; ++i) {
        s += std::to_string(v[i]);
        s += ' ';
    }
    return s;
}

int main() {
    const int n = 51;   // odd, so odd() prints one more number than even()

    std::vector<int> expected;
    for (int i = 1; i <= n; ++i) { expected.push_back(0); expected.push_back(i); }

    for (int trial = 0; trial < 20; ++trial) {
        ZeroEvenOdd z(n);
        std::vector<int> out;
        std::mutex om;

        auto record = [&](int v) {
            std::lock_guard<std::mutex> g(om);
            out.push_back(v);
            SHAKE();
        };

        // zero()'s thread is started LAST, so a solution that relies on it
        // winning the race to run first will fail here.
        std::thread te([&] { SHAKE(); z.even(record); });
        std::thread to([&] { SHAKE(); z.odd(record); });
        std::thread tz([&] { SHAKE(); z.zero(record); });
        te.join();
        to.join();
        tz.join();

        if (out != expected) {
            std::size_t k = 0;
            while (k < out.size() && k < expected.size() && out[k] == expected[k]) ++k;
            std::size_t from = k > 6 ? k - 6 : 0;
            std::printf("trial %d: sequence diverges at position %zu\n"
                        "  got      %s\n"
                        "  expected %s\n"
                        "  (%zu values produced, %zu expected)\n",
                        trial, k,
                        render(out, from, 18).c_str(),
                        render(expected, from, 18).c_str(),
                        out.size(), expected.size());
            return 1;
        }
    }

    std::printf("20 trials x n=%d: 0 before every number, every time\n", n);
    return 0;
}
