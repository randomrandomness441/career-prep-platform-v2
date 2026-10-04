#include <algorithm>
#include <vector>

long long count_submatrices(const std::vector<std::vector<int>>& grid) {
    if (grid.empty() || grid[0].empty()) return 0;
    const int rows = static_cast<int>(grid.size());
    const int cols = static_cast<int>(grid[0].size());

    std::vector<int> height(static_cast<std::size_t>(cols), 0);
    long long total = 0;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const auto cc = static_cast<std::size_t>(c);
            // Reset to 0 on a 0 cell -- this is a column of CONSECUTIVE ones ending
            // at this row, not a running total of every 1 ever seen in the column.
            height[cc] = grid[static_cast<std::size_t>(r)][cc] == 1 ? height[cc] + 1 : 0;
        }
        for (int c = 0; c < cols; ++c) {
            int min_h = height[static_cast<std::size_t>(c)];
            for (int left = c; left >= 0 && height[static_cast<std::size_t>(left)] > 0; --left) {
                min_h = std::min(min_h, height[static_cast<std::size_t>(left)]);
                total += min_h;
            }
        }
    }
    return total;
}
