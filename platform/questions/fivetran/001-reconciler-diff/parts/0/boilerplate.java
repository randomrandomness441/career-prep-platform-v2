import java.util.*;

class Solution {
    static Delta reconcile(List<Record> source, List<Record> target) {
        List<String> toInsert = new ArrayList<>();
        List<String> toUpdate = new ArrayList<>();
        List<String> toDelete = new ArrayList<>();

        for (Record s : source) {
            boolean foundInTarget = false;
            for (Record t : target) {
                if (t.id.equals(s.id)) { foundInTarget = true; break; }
            }
            if (!foundInTarget) {
                toInsert.add(s.id);
            }
            // TODO: an id present in both sides can still need an update -- you're
            // never comparing checksums here, so a changed row is silently missed.
        }
        for (Record t : target) {
            boolean foundInSource = false;
            for (Record s : source) {
                if (s.id.equals(t.id)) { foundInSource = true; break; }
            }
            if (!foundInSource) {
                toDelete.add(t.id);
            }
        }
        return new Delta(toInsert, toUpdate, toDelete);
    }
}
