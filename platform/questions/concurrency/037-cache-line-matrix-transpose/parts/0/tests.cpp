// Harness for "Cache-Line Aware Matrix Transpose". Includes the candidate's
// file verbatim.
#include "solution.hpp"

#include <cstdio>
#include <cstdint>

// No SHAKE() in this file: the layout check (Part A) is deterministic --
// it depends only on where the implementation places rows in memory -- and
// the value check (Part B) runs after every thread has already joined, so
// there's no interleaving left to perturb.

int main() {
    constexpr int kRows = 1000, kCols = 1000, kThreads = 8;
    constexpr std::size_t kCacheLine = 64;

    Matrix src(kRows, kCols);
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < kCols; ++c) src.at(r, c) = static_cast<float>(r * 10000 + c);

    Matrix dst = parallel_transpose(src, kThreads);

    // ── Part A: layout. Every internal boundary between two threads'
    // contiguous row ranges (chunk = ceil(out_rows / num_threads), the
    // convention this question specifies) must start at a 64-byte-aligned
    // address -- otherwise the last few elements of one thread's last row
    // and the first few elements of the next thread's first row land in the
    // SAME cache line, and every write from one core invalidates the
    // other's copy of it. This check is deterministic: it depends only on
    // where the implementation places rows in memory, not on scheduling.
    {
        const int out_rows = dst.rows();
        int chunk = (out_rows + kThreads - 1) / kThreads;
        bool any_straddle = false;
        for (int boundary = chunk; boundary < out_rows; boundary += chunk) {
            std::uintptr_t addr = dst.row_address(boundary);
            if (addr % kCacheLine != 0) {
                std::printf("row %d (a thread boundary) starts at address 0x%lx, which is "
                            "not 64-byte aligned -- this row shares a cache line with the "
                            "end of the previous thread's last row\n",
                            boundary, (unsigned long)addr);
                any_straddle = true;
            }
        }
        if (any_straddle) return 1;
    }

    // ── Part B: it still has to produce the right answer.
    {
        if (dst.rows() != kCols || dst.cols() != kRows) {
            std::printf("dst is %dx%d, expected %dx%d\n", dst.rows(), dst.cols(), kCols, kRows);
            return 1;
        }
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                float want = static_cast<float>(r * 10000 + c);
                float got = dst.get(c, r);
                if (got != want) {
                    std::printf("dst(%d,%d) = %.0f, expected %.0f (src(%d,%d))\n",
                                c, r, got, want, r, c);
                    return 1;
                }
            }
        }
    }

    std::printf("%dx%d transpose, %d threads: every thread boundary cache-line aligned, "
                "every element correct\n", kRows, kCols, kThreads);
    return 0;
}
