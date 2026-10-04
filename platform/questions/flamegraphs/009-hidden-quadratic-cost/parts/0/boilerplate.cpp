#include <vector>

bool seen_before(const std::vector<int>& seen, int id) {
    for (int s : seen) if (s == id) return true;
    return false;
}

std::vector<int> dedupe(const std::vector<int>& ids) {
    std::vector<int> result;
    for (int id : ids) {
        if (!seen_before(result, id)) {
            result.push_back(id);
        }
    }
    return result;
}
