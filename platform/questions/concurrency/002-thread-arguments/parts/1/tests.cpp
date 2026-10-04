#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <semaphore>
#include <string>
#include <thread>

// Recursively scribbles over the stack region make_labeler just vacated.
// A real program does this by accident simply by calling other functions.
static volatile int keep;
static void clobber(int depth) {
    char scratch[4096];
    for (size_t i = 0; i < sizeof scratch; ++i) scratch[i] = char(0xAB);
    keep = scratch[depth % 4096];
    if (depth > 0) clobber(depth - 1);
}

int main() {
    for (int trial = 0; trial < 15; ++trial) {
        Result r;
        std::binary_semaphore go{0};

        std::thread t = make_labeler(&r, &go, 7);

        clobber(24);            // the dead frame is now full of 0xAB
        SHAKE();
        go.release();           // only now is the thread allowed to read
        t.join();

        if (r.text != "worker-7") {
            std::printf("trial %d: got \"%s\", expected \"worker-7\"\n"
                        "the closure was reading a stack frame that no longer existed\n",
                        trial, r.text.c_str());
            return 1;
        }
    }
    std::printf("15 trials: label survived the caller scribbling on the dead frame\n");
    return 0;
}
