#include <string>

namespace {
// Counts how many palindromes are centered on the gap between s[lo] and
// s[hi] (pass the same index twice for an odd-length center).
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

// Counts every palindromic substring of s (including single characters).
int count_palindromic_substrings(const std::string& s) {
    // TODO: implement, using expand() above
    (void)s;
    return 0;
}
