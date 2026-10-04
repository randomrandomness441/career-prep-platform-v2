import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

class Solution {
    static List<Integer> dedupe(List<Integer> ids) {
        List<Integer> result = new ArrayList<>(ids.size());
        Set<Integer> seen = new HashSet<>();
        for (int id : ids) {
            if (seen.add(id)) {   // .add() returns false if it was already present
                result.add(id);
            }
        }
        return result;
    }
}
