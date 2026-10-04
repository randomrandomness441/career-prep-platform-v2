import java.util.*;

class Solution implements Comparator<Table> {
    private final Map<String, Integer> rank = new HashMap<>();

    Solution(List<Table> allTables) {
        Map<String, Table> byName = new HashMap<>();
        for (Table t : allTables) byName.put(t.name, t);

        List<Table> remaining = new ArrayList<>(allTables);
        Set<String> placed = new HashSet<>();
        int nextRank = 0;

        while (!remaining.isEmpty()) {
            boolean progress = false;
            for (Iterator<Table> it = remaining.iterator(); it.hasNext(); ) {
                Table t = it.next();
                boolean ready = true;
                for (String dep : t.dependsOn) {
                    if (byName.containsKey(dep) && !placed.contains(dep)) { ready = false; break; }
                }
                if (ready) {
                    rank.put(t.name, nextRank++);
                    placed.add(t.name);
                    it.remove();
                    progress = true;
                }
            }
            if (!progress) {
                List<String> stuck = new ArrayList<>();
                for (Table t : remaining) stuck.add(t.name);
                throw new IllegalArgumentException("foreign-key dependency cycle among: " + stuck);
            }
        }
    }

    public int compare(Table a, Table b) {
        return Integer.compare(rank.get(a.name), rank.get(b.name));
    }
}
