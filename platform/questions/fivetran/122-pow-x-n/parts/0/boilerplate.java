class Solution {
    public double myPow(double x, int n) {
        long exp = n < 0 ? -n : n;
        double result = 1.0;
        while (exp > 0) {
            if ((exp & 1) == 1) {
                result *= x;
            }
            x *= x;
            exp >>= 1;
        }
        return n < 0 ? 1.0 / result : result;
    }
}