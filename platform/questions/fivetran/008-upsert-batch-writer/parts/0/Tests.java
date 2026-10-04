import java.util.*;

class Row {
    final String id;
    final String parentId;
    Row(String id, String parentId) { this.id = id; this.parentId = parentId; }
}

public class Tests {
    static int fails = 0;

    static void fail(String msg) { System.out.println("FAILED: " + msg); fails++; }

    // Checks the general topological + chunking property, not one specific order --
    // more than one valid ordering can exist, and a correct solution is free to pick any.
    static void expectValidOrder(List<Row> rows, List<List<String>> batches, int batchSize, String name) {
        List<String> flat = new ArrayList<>();
        for (List<String> b : batches) {
            if (b.size() > batchSize) fail(name + ": a batch has " + b.size() + " rows, exceeds batchSize=" + batchSize);
            flat.addAll(b);
        }
        Set<String> expectedIds = new HashSet<>();
        for (Row r : rows) expectedIds.add(r.id);
        if (!new HashSet<>(flat).equals(expectedIds) || flat.size() != expectedIds.size()) {
            fail(name + ": output ids must be exactly a permutation of the input ids, got " + flat + " want a permutation of " + expectedIds);
            return;
        }
        Map<String, Integer> pos = new HashMap<>();
        for (int i = 0; i < flat.size(); i++) pos.put(flat.get(i), i);
        Set<String> ids = expectedIds;
        for (Row r : rows) {
            if (r.parentId != null && ids.contains(r.parentId)) {
                if (pos.get(r.id) <= pos.get(r.parentId)) {
                    fail(name + ": row " + r.id + " must come after its parent " + r.parentId);
                }
            }
        }
    }

    public static void main(String[] args) {
        // A straight chain, in scrambled input order -- only one valid topological order.
        List<Row> chain = List.of(new Row("c", "b"), new Row("a", null), new Row("b", "a"));
        expectValidOrder(chain, Solution.buildBatches(chain, 10), 10, "chain");

        // A parentId pointing outside the batch entirely imposes no ordering constraint.
        List<Row> external = List.of(new Row("x", "already-in-destination-table"));
        List<List<String>> extBatches = Solution.buildBatches(external, 10);
        expectValidOrder(external, extBatches, 10, "external parent");
        if (extBatches.stream().mapToInt(List::size).sum() != 1) fail("external parent: row must still appear exactly once");

        // A branching dependency tree, mixed with independent rows.
        List<Row> tree = List.of(
            new Row("b", "a"), new Row("d", null), new Row("a", null), new Row("c", "b"));
        expectValidOrder(tree, Solution.buildBatches(tree, 10), 10, "tree");

        // Chunking: 5 independent rows, batchSize=2 -> batches of size 2,2,1.
        List<Row> five = List.of(
            new Row("1", null), new Row("2", null), new Row("3", null),
            new Row("4", null), new Row("5", null));
        List<List<String>> chunked = Solution.buildBatches(five, 2);
        expectValidOrder(five, chunked, 2, "chunking");
        List<Integer> sizes = new ArrayList<>();
        for (List<String> b : chunked) sizes.add(b.size());
        if (!sizes.equals(List.of(2, 2, 1))) fail("chunking: expected batch sizes [2,2,1], got " + sizes);

        // A real cycle must be rejected, not silently dropped or looped on forever.
        List<Row> cycle = List.of(new Row("a", "c"), new Row("b", "a"), new Row("c", "b"));
        boolean threw = false;
        try {
            Solution.buildBatches(cycle, 10);
        } catch (IllegalArgumentException expected) {
            threw = true;
        }
        if (!threw) fail("cycle: must throw IllegalArgumentException instead of dropping or misordering the cyclic rows");

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
