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
        Map<String, String> schema = new LinkedHashMap<>();
        for (Map<String, Object> record : records) {
            for (Map.Entry<String, Object> e : record.entrySet()) {
                String valueType = typeOf(e.getValue());
                String existing = schema.get(e.getKey());
                schema.put(e.getKey(), existing == null ? valueType : widenType(existing, valueType));
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
