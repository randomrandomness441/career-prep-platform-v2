Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`, which also defines `FieldValue`):

```java
class FieldValue {
    final Object value;  // the leaf value itself
    final String type;   // "integer", "float", "boolean", "string", or "null"
    FieldValue(Object value, String type) { this.value = value; this.type = type; }
}

class Solution {
    static Map<String, FieldValue> flatten(Map<String, Object> record) {
        // your implementation
    }
}
```

Example:

```java
Map<String, Object> record = Map.of(
    "id", 7,
    "user", Map.of("name", "bo", "age", 30),
    "tags", List.of("x", "y"));

Map<String, FieldValue> flat = Solution.flatten(record);
// flat = {
//   "id"         -> FieldValue(7, "integer"),
//   "user__name" -> FieldValue("bo", "string"),
//   "user__age"  -> FieldValue(30, "integer"),
//   "tags__0"    -> FieldValue("x", "string"),
//   "tags__1"    -> FieldValue("y", "string"),
// }
```
