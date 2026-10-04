import java.util.*;

class Table {
    final String name;
    final List<String> dependsOn;
    Table(String name, List<String> dependsOn) { this.name = name; this.dependsOn = dependsOn; }
}

public class Tests {
    static int fails = 0;

    static void fail(String msg) { System.out.println("FAILED: " + msg); fails++; }

    static void expectValidOrder(List<Table> tables, List<Table> sorted, String name) {
        Map<String, Integer> pos = new HashMap<>();
        for (int i = 0; i < sorted.size(); i++) pos.put(sorted.get(i).name, i);
        if (pos.size() != tables.size()) {
            fail(name + ": sorted output doesn't contain the same set of tables");
            return;
        }
        Set<String> names = pos.keySet();
        for (Table t : tables) {
            for (String dep : t.dependsOn) {
                if (names.contains(dep) && pos.get(t.name) <= pos.get(dep)) {
                    fail(name + ": " + t.name + " must sort after its dependency " + dep);
                }
            }
        }
    }

    public static void main(String[] args) {
        // A direct parent/child pair, scrambled input order.
        List<Table> pair = new ArrayList<>(List.of(
            new Table("child", List.of("parent")), new Table("parent", List.of())));
        pair.sort(new Solution(pair));
        expectValidOrder(pair, pair, "pair");

        // A three-level chain -- grandchild has no DIRECT edge to grandparent, but still
        // must sort after it. This is what a purely pairwise comparator gets wrong.
        List<Table> chain = new ArrayList<>(List.of(
            new Table("grandchild", List.of("child")),
            new Table("root", List.of()),
            new Table("child", List.of("root"))));
        chain.sort(new Solution(chain));
        expectValidOrder(chain, chain, "three-level chain");

        // Multiple foreign keys on one table: order_items depends on both orders and products.
        List<Table> multi = new ArrayList<>(List.of(
            new Table("order_items", List.of("orders", "products")),
            new Table("orders", List.of("users")),
            new Table("products", List.of()),
            new Table("users", List.of())));
        multi.sort(new Solution(multi));
        expectValidOrder(multi, multi, "multiple foreign keys");

        // A dependsOn name not present among allTables imposes no constraint.
        List<Table> external = new ArrayList<>(List.of(
            new Table("x", List.of("some_table_that_already_exists"))));
        external.sort(new Solution(external));
        expectValidOrder(external, external, "external dependency");

        // A real cycle across three tables must be rejected from the constructor.
        List<Table> cycle = List.of(
            new Table("a", List.of("c")), new Table("b", List.of("a")), new Table("c", List.of("b")));
        boolean threw = false;
        try {
            new Solution(cycle);
        } catch (IllegalArgumentException expected) {
            threw = true;
        }
        if (!threw) fail("cycle: constructor must throw IllegalArgumentException instead of building a broken comparator");

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
