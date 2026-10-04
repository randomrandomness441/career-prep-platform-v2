Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`).

```java
class Solution {
    static String widenType(String existingType, String incomingType) { ... }
    static Map<String, String> inferSchema(List<Map<String, Object>> records) { ... }
}
```

A value's own type -- what you pass as `incomingType` when you call `widenType` yourself,
and what `inferSchema` computes internally for each value -- is one of `"integer"` (Java
`Integer`/`Long`), `"float"` (`Double`/`Float`), `"boolean"` (`Boolean`), `"string"`
(`String`), or `"null"`.

Example:

```java
List<Map<String, Object>> records = List.of(
    Map.of("id", 1, "amount", 10),
    Map.of("id", 2, "amount", 10.5),
    Map.of("id", 3, "amount", "N/A"));

Solution.inferSchema(records);
// {"id" -> "integer", "amount" -> "string"}
// amount: integer (rec 1) widened with float (rec 2) -> float,
// then widened with string (rec 3) -> string.
```
