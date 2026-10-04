import java.util.ArrayList;
import java.util.List;

class Solution {
    static boolean seenBefore(List<Integer> seen, int id) {
        for (int s : seen) if (s == id) return true;
        return false;
    }

    static List<Integer> dedupe(List<Integer> ids) {
        List<Integer> result = new ArrayList<>();
        for (int id : ids) {
            if (!seenBefore(result, id)) {
                result.add(id);
            }
        }
        return result;
    }
}
