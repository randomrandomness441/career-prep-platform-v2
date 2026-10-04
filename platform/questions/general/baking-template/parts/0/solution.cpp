#include <array>
#include <string>
#include <utility>
#include <vector>

namespace {
bool matches(const std::string& box, const std::string& tmpl) {
    if (box.size() != tmpl.size()) return false;
    std::array<int, 26> count{};
    for (char c : box) ++count[static_cast<std::size_t>(c - 'a')];
    for (char c : tmpl) --count[static_cast<std::size_t>(c - 'a')];
    for (int n : count)
        if (n != 0) return false;
    return true;
}
}  // namespace

int count_matching_pairs(const std::vector<std::pair<std::string, std::string>>& items) {
    int total = 0;
    for (const auto& [box, tmpl] : items)
        if (matches(box, tmpl)) ++total;
    return total;
}
