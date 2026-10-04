// This is a conceptual (Q&A) question -- there's no coding exercise, so this harness
// just checks whether the placeholder has been "answered" (i.e. Reading has been
// worked through). Includes the candidate's file verbatim.
#include "solution.hpp"
#include <cstdio>

int main() {
    if (!answered()) {
        std::printf("This is a conceptual question -- there's no code to complete. "
                    "Read the Problem tab, then check Reading for the full answer.\n");
        return 1;
    }
    std::printf("see the Reading tab for the full written answer\n");
    return 0;
}
