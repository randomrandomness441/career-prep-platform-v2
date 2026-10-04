#include "solution.hpp"

#include "concur/stress.hpp"
#include <cstdio>
#include <thread>

int main() {
    for (int trial = 0; trial < 20; ++trial) {
        SHAKE();
        Counter a;
        launch_lambda(a, 1000);
        if (a.value != 1000) {
            std::printf("launch_lambda: caller's counter is %d, expected 1000 "
                        "(the thread updated a copy)\n", a.value);
            return 1;
        }
        Counter b;
        launch_function(b, 1000);
        if (b.value != 1000) {
            std::printf("launch_function: caller's counter is %d, expected 1000\n", b.value);
            return 1;
        }
    }
    std::printf("20 trials: both forms updated the caller's counter\n");
    return 0;
}
