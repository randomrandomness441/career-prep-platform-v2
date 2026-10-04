Your submission is a plain function, same convention as every other coding question in
this platform:

```cpp
#include <vector>

std::vector<int> dedupe(const std::vector<int>& ids) {
    // your implementation
}
```

To actually profile the given version yourself before fixing it, drop it into a small
standalone program on your own machine:

```cpp
#include <vector>
#include <cstdio>

bool seen_before(const std::vector<int>& seen, int id) {
    for (int s : seen) if (s == id) return true;
    return false;
}
std::vector<int> dedupe(const std::vector<int>& ids) {
    std::vector<int> result;
    for (int id : ids) if (!seen_before(result, id)) result.push_back(id);
    return result;
}

int main() {
    std::vector<int> ids;
    for (int i = 0; i < 300000; i++) ids.push_back(rand() % 300000);
    auto r = dedupe(ids);
    printf("%zu unique\n", r.size());
}
```

Compile it (`clang++ -O1 -g -o prog prog.cpp`) and profile the running binary with
whatever sampling profiler you have — macOS's built-in `sample`, `samply`, or Linux
`perf`. Then come back and fix the real submission above.

## A real profile, already captured

`dedupe-naive.samply.json.gz`, next to this file, is a real `samply` capture of the
exact naive version above, compiled and run for real on this machine. View it
yourself:

```
samply load dedupe-naive.samply.json.gz
```

This starts a local server and opens the capture in the real Firefox Profiler UI in
your browser — a different, more full-featured tool than the one used for the Java
version of this question (async-profiler), worth being familiar with both. Note: this
step needs your own regular browser (`samply load` talks to `127.0.0.1`) — it can't be
verified through a sandboxed browser tool, only confirmed here that the capture itself
is real, valid data.

