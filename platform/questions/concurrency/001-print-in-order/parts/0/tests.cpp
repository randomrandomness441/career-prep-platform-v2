// Harness for Print in Order. Includes the candidate's class verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

int main() {
    // The launch order is shuffled every trial. A solution that only works
    // when the OS happens to start first() first will fail here.
    std::mt19937 rng(12345);

    for (int trial = 0; trial < 60; ++trial) {
        Foo foo;
        std::string out;
        std::mutex om;
        auto record = [&](const char* s) {
            std::lock_guard<std::mutex> g(om);
            out += s;
        };

        std::vector<int> order{0, 1, 2};
        std::shuffle(order.begin(), order.end(), rng);

        std::vector<std::thread> ts;
        for (int which : order) {
            ts.emplace_back([&, which] {
                SHAKE();
                if (which == 0)      foo.first ([&]{ record("first");  });
                else if (which == 1) foo.second([&]{ record("second"); });
                else                 foo.third ([&]{ record("third");  });
            });
        }
        for (auto& t : ts) t.join();

        if (out != "firstsecondthird") {
            std::printf("trial %d produced \"%s\", expected \"firstsecondthird\"\n",
                        trial, out.c_str());
            return 1;
        }
    }
    std::printf("60 trials, every one ordered correctly\n");
    return 0;
}
