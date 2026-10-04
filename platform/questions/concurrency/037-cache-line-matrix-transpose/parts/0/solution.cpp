#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <thread>
#include <vector>

// A row-major matrix whose storage is 64-byte aligned AND whose row stride
// is padded up to a multiple of 64 bytes (16 floats). That second part is
// the actual point: without it, a row whose true width isn't a multiple of
// the cache line size drifts out of alignment with every row after it, so
// SOME row boundary is eventually guaranteed to land in the middle of a
// cache line no matter how the rows are grouped between threads. Padding
// every row's stride means every row starts at the beginning of a fresh
// cache line, so any split that hands out whole rows to each thread can
// never have two threads writing into the same line.
class Matrix {
public:
    Matrix(int rows, int cols) : rows_(rows), cols_(cols) {
        constexpr std::size_t kCacheLineFloats = 64 / sizeof(float);
        stride_ = ((static_cast<std::size_t>(cols) + kCacheLineFloats - 1) /
                   kCacheLineFloats) * kCacheLineFloats;
        std::size_t n = stride_ * static_cast<std::size_t>(rows == 0 ? 0 : rows);
        data_ = AlignedBuf(n == 0 ? nullptr
                                  : static_cast<float*>(::operator new(
                                        n * sizeof(float), std::align_val_t(64))));
    }

    float& at(int r, int c) { return data_[static_cast<std::size_t>(r) * stride_ + static_cast<std::size_t>(c)]; }
    float get(int r, int c) const { return data_[static_cast<std::size_t>(r) * stride_ + static_cast<std::size_t>(c)]; }
    int rows() const { return rows_; }
    int cols() const { return cols_; }

    // Instrumentation: the address of row r's first element -- what the
    // tests inspect to confirm two threads' row ranges never share a line.
    std::uintptr_t row_address(int r) const {
        return reinterpret_cast<std::uintptr_t>(data_.get() + static_cast<std::size_t>(r) * stride_);
    }

private:
    struct Deleter { void operator()(float* p) const { ::operator delete(p, std::align_val_t(64)); } };
    using AlignedBuf = std::unique_ptr<float[], Deleter>;

    int rows_, cols_;
    std::size_t stride_;
    AlignedBuf data_;
};

Matrix parallel_transpose(const Matrix& src, int num_threads = 8) {
    Matrix dst(src.cols(), src.rows());
    const int out_rows = dst.rows();
    num_threads = std::max(1, std::min(num_threads, out_rows == 0 ? 1 : out_rows));
    int chunk = (out_rows + num_threads - 1) / num_threads;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        int lo = t * chunk;
        int hi = std::min(lo + chunk, out_rows);
        threads.emplace_back([&src, &dst, lo, hi] {
            for (int r = lo; r < hi; ++r)
                for (int c = 0; c < dst.cols(); ++c) dst.at(r, c) = src.get(c, r);
        });
    }
    for (auto& th : threads) th.join();
    return dst;
}
