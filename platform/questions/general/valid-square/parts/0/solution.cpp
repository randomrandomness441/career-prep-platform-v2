#include <array>
#include <map>

struct Point { int x, y; };

namespace {
long dist2(const Point& a, const Point& b) {
    const long dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}
}  // namespace

bool is_valid_square(Point p1, Point p2, Point p3, Point p4) {
    const std::array<Point, 4> pts{p1, p2, p3, p4};

    // All 6 pairwise squared distances, order-independent -- this is what makes the
    // check work no matter what order the 4 points were handed to us in.
    std::map<long, int> counts;
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
            ++counts[dist2(pts[i], pts[j])];

    if (counts.size() != 2) return false;   // a square has exactly 2 distinct distances

    auto it = counts.begin();
    const long side = it->first;
    const int side_count = it->second;
    ++it;
    const long diag = it->first;
    const int diag_count = it->second;

    return side > 0 && diag == 2 * side && side_count == 4 && diag_count == 2;
}
