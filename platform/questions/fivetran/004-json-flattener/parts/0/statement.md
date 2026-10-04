A source API returns nested JSON objects, but the destination is a relational table: one
row, flat columns. You need to turn a nested record into a flat set of columns, naming
each one after the path that reaches it.

Implement:

```java
static Map<String, FieldValue> flatten(Map<String, Object> record)
```

The input is a parsed JSON object: a `Map<String, Object>` whose values are themselves
`Map<String, Object>` (nested object), `List<Object>` (array), or a leaf (`String`,
`Integer`/`Long`, `Double`/`Float`, `Boolean`, or `null`).

Produce one entry per leaf value, keyed by its full path with `__` joining each segment:

- A nested object's keys join with their parent: `{"user": {"name": "bo"}}` becomes key `"user__name"`.
- An array is indexed by position, not exploded into separate rows: `{"tags": ["a","b"]}` becomes keys `"tags__0"` and `"tags__1"`. An array of objects indexes then continues joining: `{"items": [{"id": 1}]}` becomes `"items__0__id"`.
- Each leaf's `FieldValue` carries the value itself plus its inferred type name: `"integer"`, `"float"`, `"boolean"`, `"string"`, or `"null"`.
- An empty object or empty array contributes no keys at all.

### Constraints

- Nesting can go arbitrarily deep (objects inside arrays inside objects, etc.) -- the intended solution is recursive, not hardcoded to 2 or 3 levels.
- A path with no leaf under it (empty object/array anywhere in the tree) must not appear in the output, at any depth.
