#include <set>
#include <utility>
#include <vector>

std::vector<std::pair<int, int>> circle_points(int cx, int cy, int r) {
    std::set<std::pair<int, int>> pts;
    if (r == 0) {
        pts.insert({cx, cy});
        return {pts.begin(), pts.end()};
    }

    auto plot8 = [&](int x, int y) {
        pts.insert({cx + x, cy + y});
        pts.insert({cx - x, cy + y});
        pts.insert({cx + x, cy - y});
        pts.insert({cx - x, cy - y});
        pts.insert({cx + y, cy + x});
        pts.insert({cx - y, cy + x});
        pts.insert({cx + y, cy - x});
        pts.insert({cx - y, cy - x});
    };

    // Midpoint circle algorithm: compute one octant (45 deg to 90 deg, x >= y) with
    // pure integer arithmetic, then mirror it 8 ways using the circle's symmetry.
    int x = r, y = 0;
    int d = 1 - r;
    while (x >= y) {
        plot8(x, y);
        ++y;
        if (d <= 0) {
            d += 2 * y + 1;          // midpoint is inside the circle: move up only
        } else {
            --x;
            d += 2 * (y - x) + 1;    // midpoint is outside: move up AND inward
        }
    }
    return {pts.begin(), pts.end()};
}
