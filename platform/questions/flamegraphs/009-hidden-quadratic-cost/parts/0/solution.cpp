#include <vector>
#include <unordered_set>

std::vector<int> dedupe(const std::vector<int>& ids) {
    std::vector<int> result;
    std::unordered_set<int> seen;
    result.reserve(ids.size());
    for (int id : ids) {
        if (seen.insert(id).second) {   // .second is true only on a genuinely new id
            result.push_back(id);
        }
    }
    return result;
}
