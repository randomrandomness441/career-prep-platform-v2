class Solution {
    public boolean searchMatrix(int[][] matrix, int target) {
        int m = matrix.length;
        int n = matrix[0].length;

        // Binary search for the row that could hold the target.
        int lo = 0, hi = m - 1;
        while (lo < hi) {
            int mid = (lo + hi + 1) / 2;
            if (matrix[mid][0] < target) {
                lo = mid;
            } else {
                hi = mid - 1;
            }
        }

        // Binary search inside that row.
        int[] row = matrix[lo];
        int l = 0, r = n - 1;
        while (l <= r) {
            int mid = (l + r) / 2;
            if (row[mid] == target) {
                return true;
            }
            if (row[mid] < target) {
                l = mid + 1;
            } else {
                r = mid - 1;
            }
        }
        return false;
    }
}