// Follow-up 1: a throwing callback must not strand the other threads.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Runs one trial. `throw_at` picks which callback explodes (-1 = none).
// Returns the printed sequence; sets `escaped` if the exception reached us.
static std::string trial(int throw_at, bool& escaped, std::mt19937& rng) {
    Foo foo;
    std::string out;
    std::mutex om;
    escaped = false;
    auto record = [&](const char* s) { std::lock_guard<std::mutex> g(om); out += s; };

    auto body = [&](int which) {
        auto cb = [&](const char* name) {
            return [&, name] {
                if (which == throw_at) throw std::runtime_error("printer offline");
                record(name);
            };
        };
        try {
            if (which == 0)      foo.first (cb("first"));
            else if (which == 1) foo.second(cb("second"));
            else                 foo.third (cb("third"));
        } catch (const std::runtime_error&) {
            std::lock_guard<std::mutex> g(om);
            escaped = true;
        }
    };

    std::vector<int> order{0, 1, 2};
    std::shuffle(order.begin(), order.end(), rng);
    std::vector<std::thread> ts;
    for (int w : order) ts.emplace_back([&, w] { SHAKE(); body(w); });
    for (auto& t : ts) t.join();     // hangs here if a gate was never opened
    return out;
}

int main() {
    std::mt19937 rng(999);

    for (int i = 0; i < 25; ++i) {
        bool esc = false;
        std::string s = trial(-1, esc, rng);
        if (s != "firstsecondthird") {
            std::printf("no-throw case broke: got \"%s\"\n", s.c_str()); return 1;
        }
        if (esc) { std::printf("nothing threw, but an exception escaped\n"); return 1; }
    }

    // printFirst throws: first prints nothing, the other two must still run.
    for (int i = 0; i < 25; ++i) {
        bool esc = false;
        std::string s = trial(0, esc, rng);
        if (!esc) { std::printf("printFirst threw but nothing propagated\n"); return 1; }
        if (s != "secondthird") {
            std::printf("printFirst threw: expected \"secondthird\", got \"%s\"\n", s.c_str());
            return 1;
        }
    }

    // printSecond throws: third must not be stranded behind it.
    for (int i = 0; i < 25; ++i) {
        bool esc = false;
        std::string s = trial(1, esc, rng);
        if (!esc) { std::printf("printSecond threw but nothing propagated\n"); return 1; }
        if (s != "firstthird") {
            std::printf("printSecond threw: expected \"firstthird\", got \"%s\"\n", s.c_str());
            return 1;
        }
    }

    std::printf("75 trials: exceptions propagate, nobody hangs\n");
    return 0;
}
