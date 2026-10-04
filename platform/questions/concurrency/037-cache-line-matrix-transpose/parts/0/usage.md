### How it's called

```cpp
Matrix src(1000, 500);
// ... fill src via src.at(r, c) = ... ...

Matrix out = parallel_transpose(src, /*num_threads=*/8);
// out.rows() == 500, out.cols() == 1000
// out.get(c, r) == src.get(r, c) for every r, c

// row ranges assigned to different threads never share a cache line:
std::uintptr_t a = out.row_address(62);    // last row of thread 0's chunk (chunk=63)
std::uintptr_t b = out.row_address(63);    // first row of thread 1's chunk
// a and b must not fall in the same 64-byte-aligned block
```

A single call, internally spreading the output rows across `num_threads` threads it
creates and joins itself, in the required contiguous-chunk convention.
