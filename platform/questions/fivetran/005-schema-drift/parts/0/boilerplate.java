import java.util.*;

class Solution {
    static String widenType(String existingType, String incomingType) {
        if (existingType.equals(incomingType)) return existingType;
        if (existingType.equals("null")) return incomingType;
        if (incomingType.equals("null")) return existingType;
        Set<String> both = new HashSet<>(List.of(existingType, incomingType));
        if (both.equals(new HashSet<>(List.of("integer", "float")))) return "float";
        return "string";
    }

    static Map<String, String> inferSchema(List<Map<String, Object>> records) {
        // TODO: only ever sets a field's type the first time it's seen. Every later
        // record is folded into the "already know this field" branch and silently
        // ignored -- a field that drifts from integer to string never gets widened,
        // the schema just keeps reporting the type of the very first sample.
        Map<String, String> schema = new LinkedHashMap<>();
        for (Map<String, Object> record : records) {
            for (Map.Entry<String, Object> e : record.entrySet()) {
                if (!schema.containsKey(e.getKey())) {
                    schema.put(e.getKey(), typeOf(e.getValue()));
                }
            }
        }
        return schema;
    }

    private static String typeOf(Object v) {
        if (v == null) return "null";
        if (v instanceof Boolean) return "boolean";
        if (v instanceof Integer || v instanceof Long) return "integer";
        if (v instanceof Double || v instanceof Float) return "float";
        return "string";
    }
}
