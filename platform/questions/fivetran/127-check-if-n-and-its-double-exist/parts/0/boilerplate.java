import java.util.HashSet;
import java.util.Set;

class Solution {
    public boolean checkIfExist(int[] arr) {
        Set<Integer> seen = new HashSet<>();
        for (int x : arr) {
            seen.add(x);
        }
        for (int x : arr) {
            if (seen.contains(2 * x)) {
                return true;
            }
        }
        return false;
    }
}