import java.util.HashSet;
import java.util.Set;

class Solution {
    public boolean checkIfExist(int[] arr) {
        Set<Integer> seen = new HashSet<>();
        for (int x : arr) {
            if (seen.contains(2 * x) || (x % 2 == 0 && seen.contains(x / 2))) {
                return true;
            }
            seen.add(x);
        }
        return false;
    }
}