import java.util.*;

class FieldValue {
    final Object value;
    final String type;
    FieldValue(Object value, String type) { this.value = value; this.type = type; }
}

public class Tests {
    static int fails = 0;

    static Map<String, Object> mapOf(Object... kv) {
        LinkedHashMap<String, Object> m = new LinkedHashMap<>();
        for (int i = 0; i < kv.length; i += 2) m.put((String) kv[i], kv[i + 1]);
        return m;
    }

    static void expectField(Map<String, FieldValue> got, String path, Object value, String type) {
        FieldValue f = got.get(path);
        if (f == null) {
            System.out.println("FAILED: missing key \"" + path + "\" (have " + got.keySet() + ")");
            fails++;
            return;
        }
        if (!Objects.equals(f.value, value) || !type.equals(f.type)) {
            System.out.printf("FAILED: \"%s\" got (%s, %s) want (%s, %s)%n", path, f.value, f.type, value, type);
            fails++;
        }
    }

    static void expectAbsent(Map<String, FieldValue> got, String prefix) {
        for (String k : got.keySet()) {
            if (k.equals(prefix) || k.startsWith(prefix + "__")) {
                System.out.println("FAILED: key \"" + k + "\" should not exist (empty subtree under \"" + prefix + "\")");
                fails++;
                return;
            }
        }
    }

    public static void main(String[] args) {
        // Flat record, no nesting at all.
        Map<String, FieldValue> flat = Solution.flatten(mapOf("a", 1, "b", "x"));
        expectField(flat, "a", 1, "integer");
        expectField(flat, "b", "x", "string");
        if (flat.size() != 2) { System.out.println("FAILED: flat record has extra keys: " + flat.keySet()); fails++; }

        // One level of object nesting.
        Map<String, FieldValue> nested = Solution.flatten(
            mapOf("user", mapOf("name", "bo", "age", 30)));
        expectField(nested, "user__name", "bo", "string");
        expectField(nested, "user__age", 30, "integer");

        // Array of primitives, indexed by position.
        Map<String, FieldValue> arr = Solution.flatten(mapOf("tags", List.of("x", "y", "z")));
        expectField(arr, "tags__0", "x", "string");
        expectField(arr, "tags__1", "y", "string");
        expectField(arr, "tags__2", "z", "string");

        // Array of objects: index then keep joining.
        Map<String, FieldValue> items = Solution.flatten(mapOf(
            "items", List.of(mapOf("id", 1, "name", "a"), mapOf("id", 2, "name", "b"))));
        expectField(items, "items__0__id", 1, "integer");
        expectField(items, "items__0__name", "a", "string");
        expectField(items, "items__1__id", 2, "integer");
        expectField(items, "items__1__name", "b", "string");

        // null, float, boolean type inference.
        Map<String, Object> typesRecord = mapOf("deleted_at", null, "price", 9.99, "active", true);
        Map<String, FieldValue> types = Solution.flatten(typesRecord);
        expectField(types, "deleted_at", null, "null");
        expectField(types, "price", 9.99, "float");
        expectField(types, "active", true, "boolean");

        // Empty object and empty array contribute no keys, at any depth.
        Map<String, FieldValue> empties = Solution.flatten(mapOf(
            "meta", mapOf(), "list", List.of(), "real", 1,
            "wrapper", mapOf("inner_empty", mapOf())));
        expectAbsent(empties, "meta");
        expectAbsent(empties, "list");
        expectAbsent(empties, "wrapper__inner_empty");
        expectField(empties, "real", 1, "integer");

        // Deep nesting: object inside array inside object inside array.
        Map<String, FieldValue> deep = Solution.flatten(mapOf(
            "a", List.of(mapOf("b", List.of(mapOf("c", 42))))));
        expectField(deep, "a__0__b__0__c", 42, "integer");

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
