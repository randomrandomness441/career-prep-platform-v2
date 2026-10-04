// Harness for Print FooBar Alternately. Includes the candidate's class verbatim.
#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

int main() {
    const int n = 100;

    std::string expected;
    for (int i = 0; i < n; ++i) expected += "foobar";

    for (int trial = 0; trial < 20; ++trial) {
        FooBar fb(n);
        std::string out;
        std::mutex om;

        // bar's thread is deliberately started FIRST. A solution that only
        // works because foo happens to get scheduled first will fail here.
        auto record = [&](const char* s) {
            std::lock_guard<std::mutex> g(om);
            out += s;
            SHAKE();
        };

        std::thread tb([&] { SHAKE(); fb.bar([&] { record("bar"); }); });
        std::thread tf([&] { SHAKE(); fb.foo([&] { record("foo"); }); });
        tb.join();
        tf.join();

        if (out != expected) {
            std::size_t k = 0;
            while (k < out.size() && k < expected.size() && out[k] == expected[k]) ++k;
            std::printf("trial %d: output diverges at character %zu\n"
                        "  got      %s\n"
                        "  expected %s\n"
                        "  (%zu characters produced, %zu expected)\n",
                        trial, k,
                        out.substr(k > 9 ? k - 9 : 0, 30).c_str(),
                        expected.substr(k > 9 ? k - 9 : 0, 30).c_str(),
                        out.size(), expected.size());
            return 1;
        }
    }

    std::printf("20 trials x %d rounds: strict foo/bar alternation every time\n", n);
    return 0;
}
