import java.util.*;

class Solution implements Comparator<Table> {
    Solution(List<Table> allTables) {
        // no cycle check -- construction always succeeds
    }

    // TODO: only looks at a DIRECT foreign key between the two tables being compared, so
    // it has no idea about the rest of the graph. A grandchild table has no direct edge
    // to its grandparent, so this reports them as "equal" and Collections.sort is free to
    // leave them in the wrong order. A cycle spread across three or more tables never
    // trips anything either -- no single pair has a direct edge in both directions.
    public int compare(Table a, Table b) {
        if (a.dependsOn.contains(b.name)) return 1;
        if (b.dependsOn.contains(a.name)) return -1;
        return 0;
    }
}
