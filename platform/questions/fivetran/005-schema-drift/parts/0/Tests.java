import java.util.*;

public class Tests {
    static int fails = 0;

    static Map<String, Object> mapOf(Object... kv) {
        LinkedHashMap<String, Object> m = new LinkedHashMap<>();
        for (int i = 0; i < kv.length; i += 2) m.put((String) kv[i], kv[i + 1]);
        return m;
    }

    static void expectEq(String got, String want, String name) {
        if (!Objects.equals(got, want)) {
            System.out.printf("FAILED %s: got %s want %s%n", name, got, want);
            fails++;
        }
    }

    public static void main(String[] args) {
        // widenType: direct lattice checks.
        expectEq(Solution.widenType("integer", "integer"), "integer", "same type stays");
        expectEq(Solution.widenType("integer", "float"), "float", "int+float -> float");
        expectEq(Solution.widenType("float", "integer"), "float", "float+int -> float (symmetric)");
        expectEq(Solution.widenType("integer", "string"), "string", "int+string -> string");
        expectEq(Solution.widenType("boolean", "integer"), "string", "boolean+int -> string");
        expectEq(Solution.widenType("boolean", "boolean"), "boolean", "boolean+boolean stays");
        expectEq(Solution.widenType("null", "integer"), "integer", "null+int -> int");
        expectEq(Solution.widenType("integer", "null"), "integer", "int+null -> int");
        expectEq(Solution.widenType("string", "float"), "string", "string+float -> string");

        // inferSchema: a field drifting integer -> float -> string across records.
        List<Map<String, Object>> drift = List.of(
            mapOf("id", 1, "amount", 10),
            mapOf("id", 2, "amount", 10.5),
            mapOf("id", 3, "amount", "N/A"));
        Map<String, String> s1 = Solution.inferSchema(drift);
        expectEq(s1.get("id"), "integer", "drift: id stays integer");
        expectEq(s1.get("amount"), "string", "drift: amount widens int -> float -> string");

        // A record missing a field doesn't affect that field's type.
        List<Map<String, Object>> sparse = List.of(
            mapOf("a", 1, "b", "x"),
            mapOf("a", 2));
        Map<String, String> s2 = Solution.inferSchema(sparse);
        expectEq(s2.get("a"), "integer", "sparse: a stays integer across both records");
        expectEq(s2.get("b"), "string", "sparse: b keeps its type even though record 2 omits it");

        // A leading null doesn't lock the type -- the first real value decides it.
        List<Map<String, Object>> nullFirst = List.of(mapOf("x", null), mapOf("x", 5));
        expectEq(Solution.inferSchema(nullFirst).get("x"), "integer", "null then int -> int");

        // Every value null -> the field's type is genuinely unresolved.
        List<Map<String, Object>> allNull = List.of(mapOf("x", null), mapOf("x", null));
        expectEq(Solution.inferSchema(allNull).get("x"), "null", "all-null field stays null");

        // boolean meeting a non-boolean, non-null type widens to string, same as any
        // other incompatible pair -- not a special case.
        List<Map<String, Object>> boolDrift = List.of(mapOf("flag", true), mapOf("flag", 1));
        expectEq(Solution.inferSchema(boolDrift).get("flag"), "string", "boolean then int -> string");

        if (fails > 0) { System.out.println(fails + " check(s) failed"); System.exit(1); }
        System.out.println("all checks passed");
    }
}
