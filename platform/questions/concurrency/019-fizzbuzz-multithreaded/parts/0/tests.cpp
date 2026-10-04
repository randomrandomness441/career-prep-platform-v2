// Harness for FizzBuzz Multithreaded. Includes the candidate's class verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static std::string join(const std::vector<std::string>& v, std::size_t from, std::size_t count) {
    std::string s;
    for (std::size_t i = from; i < v.size() && i < from + count; ++i) {
        s += v[i];
        s += ' ';
    }
    return s;
}

int main() {
    const int n = 45;   // covers 15, 30 and 45, so fizzbuzz() runs three times

    std::vector<std::string> expected;
    for (int i = 1; i <= n; ++i) {
        if      (i % 15 == 0) expected.push_back("fizzbuzz");
        else if (i % 3  == 0) expected.push_back("fizz");
        else if (i % 5  == 0) expected.push_back("buzz");
        else                  expected.push_back(std::to_string(i));
    }

    for (int trial = 0; trial < 10; ++trial) {
        FizzBuzz fb(n);
        std::vector<std::string> out;
        std::mutex om;

        auto record = [&](std::string s) {
            std::lock_guard<std::mutex> g(om);
            out.push_back(std::move(s));
            SHAKE();
        };

        // number() is started last on purpose: it owns value 1, so a solution
        // that assumes the first thread launched gets to go first will fail.
        std::thread t_fb([&] { SHAKE(); fb.fizzbuzz([&] { record("fizzbuzz"); }); });
        std::thread t_b ([&] { SHAKE(); fb.buzz    ([&] { record("buzz");     }); });
        std::thread t_f ([&] { SHAKE(); fb.fizz    ([&] { record("fizz");     }); });
        std::thread t_n ([&] { SHAKE(); fb.number  ([&](int v) { record(std::to_string(v)); }); });
        t_fb.join();
        t_b.join();
        t_f.join();
        t_n.join();

        if (out != expected) {
            std::size_t k = 0;
            while (k < out.size() && k < expected.size() && out[k] == expected[k]) ++k;
            std::size_t from = k > 4 ? k - 4 : 0;
            std::printf("trial %d: sequence diverges at position %zu\n"
                        "  got      %s\n"
                        "  expected %s\n"
                        "  (%zu tokens produced, %zu expected)\n",
                        trial, k,
                        join(out, from, 12).c_str(),
                        join(expected, from, 12).c_str(),
                        out.size(), expected.size());
            return 1;
        }
    }

    // n = 0: nothing to print, but all four calls must still return.
    {
        FizzBuzz fb(0);
        std::vector<std::string> out;
        std::mutex om;
        auto record = [&](std::string s) {
            std::lock_guard<std::mutex> g(om);
            out.push_back(std::move(s));
        };
        std::thread a([&] { fb.fizz    ([&] { record("fizz");     }); });
        std::thread b([&] { fb.buzz    ([&] { record("buzz");     }); });
        std::thread c([&] { fb.fizzbuzz([&] { record("fizzbuzz"); }); });
        std::thread d([&] { fb.number  ([&](int v) { record(std::to_string(v)); }); });
        a.join(); b.join(); c.join(); d.join();
        if (!out.empty()) {
            std::printf("n = 0 produced %zu tokens, expected none\n", out.size());
            return 1;
        }
    }

    std::printf("10 trials x n=%d in order, and n=0 returns all four threads\n", n);
    return 0;
}
