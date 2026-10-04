import java.util.*;

class Solution {
    static Map<String, FieldValue> flatten(Map<String, Object> record) {
        Map<String, FieldValue> out = new LinkedHashMap<>();
        flattenInto("", record, out);
        return out;
    }

    private static void flattenInto(String prefix, Object value, Map<String, FieldValue> out) {
        if (value instanceof Map) {
            for (Map.Entry<?, ?> e : ((Map<?, ?>) value).entrySet()) {
                String key = (String) e.getKey();
                String path = prefix.isEmpty() ? key : prefix + "__" + key;
                flattenInto(path, e.getValue(), out);
            }
            return;
        }
        // TODO: arrays aren't treated as another level to flatten -- a List falls straight
        // through to the leaf branch below and gets stored whole under its parent's key,
        // so "tags": ["x","y"] never becomes tags__0 / tags__1.
        out.put(prefix, new FieldValue(value, inferType(value)));
    }

    private static String inferType(Object v) {
        if (v == null) return "null";
        if (v instanceof Boolean) return "boolean";
        if (v instanceof Integer || v instanceof Long) return "integer";
        if (v instanceof Double || v instanceof Float) return "float";
        return "string";
    }
}
