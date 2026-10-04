#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <thread>
#include <vector>

// A row-major matrix, 64-byte aligned storage. parallel_transpose splits
// whole rows across threads -- no two threads ever touch the same row.
class Matrix {
public:
    Matrix(int rows, int cols) : rows_(rows), cols_(cols) {
        std::size_t n = static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows == 0 ? 0 : rows);
        data_ = AlignedBuf(n == 0 ? nullptr
                                  : static_cast<float*>(::operator new(
                                        n * sizeof(float), std::align_val_t(64))));
    }

    float& at(int r, int c) { return data_[static_cast<std::size_t>(r) * static_cast<std::size_t>(cols_) + static_cast<std::size_t>(c)]; }
    float get(int r, int c) const { return data_[static_cast<std::size_t>(r) * static_cast<std::size_t>(cols_) + static_cast<std::size_t>(c)]; }
    int rows() const { return rows_; }
    int cols() const { return cols_; }

    // TODO: does every row start at the beginning of a fresh 64-byte cache
    //       line, or can a row boundary land in the middle of one?
    std::uintptr_t row_address(int r) const {
        return reinterpret_cast<std::uintptr_t>(data_.get() + static_cast<std::size_t>(r) * static_cast<std::size_t>(cols_));
    }

private:
    struct Deleter { void operator()(float* p) const { ::operator delete(p, std::align_val_t(64)); } };
    using AlignedBuf = std::unique_ptr<float[], Deleter>;

    int rows_, cols_;
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
