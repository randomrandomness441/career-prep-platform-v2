#include <string>
#include <utility>
#include <vector>

namespace {
// True if `box`'s letters can be rearranged to exactly match `tmpl`'s
// letters -- same multiset of characters, same counts.
bool matches(const std::string& box, const std::string& tmpl) {
    // TODO: implement
    (void)box;
    (void)tmpl;
    return false;
}
}  // namespace

int count_matching_pairs(const std::vector<std::pair<std::string, std::string>>& items) {
    int total = 0;
    for (const auto& [box, tmpl] : items)
        if (matches(box, tmpl)) ++total;
    return total;
}
