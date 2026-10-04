A source API doesn't publish a schema. A connector has to infer one by watching records go
by, field by field. The catch: the source's own schema drifts over time (a field that was
always an integer starts showing up as a float, or as a string), and you can't just crash
the sync every time that happens.

Implement two static methods on `Solution`:

    static String widenType(String existingType, String incomingType)

    static Map<String, String> inferSchema(List<Map<String, Object>> records)

`widenType` decides what a field's type becomes when a new value's type doesn't match what
was inferred before. Use this widening policy:

- Widening with `"null"` on either side keeps the other type (a null value never narrows or
  changes what's already known).
- `"integer"` widening with `"float"` becomes `"float"` (every integer is representable as
  a float; the reverse isn't safe).
- Any other mismatch (including `"boolean"` meeting anything but itself or null) becomes
  `"string"` -- the one type that can hold any value, used as the universal fallback.
- Matching types stay as they are.

`inferSchema` walks `records` in order and, for every field on every record, folds its
value's type into the running schema with `widenType`. A field's type in the result is
`"integer"`, `"float"`, `"boolean"`, `"string"`, or `"null"` (only if every value seen for
that field, across every record, was null).

**Constraints**

- Not every record has every field. A record missing a field doesn't affect that field's
  type at all -- it's simply not folded in for that record.
- Field order in the output doesn't matter, only the final field -> type mapping.
