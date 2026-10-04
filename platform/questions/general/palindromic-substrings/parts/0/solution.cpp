#include <string>

namespace {
int expand(const std::string& s, int lo, int hi) {
    int count = 0;
    while (lo >= 0 && hi < static_cast<int>(s.size()) && s[static_cast<std::size_t>(lo)] == s[static_cast<std::size_t>(hi)]) {
        ++count;
        --lo;
        ++hi;
    }
    return count;
}
}  // namespace

int count_palindromic_substrings(const std::string& s) {
    int total = 0;
    for (int i = 0; i < static_cast<int>(s.size()); ++i) {
        total += expand(s, i, i);        // odd-length centers
        total += expand(s, i, i + 1);    // even-length centers
    }
    return total;
}
