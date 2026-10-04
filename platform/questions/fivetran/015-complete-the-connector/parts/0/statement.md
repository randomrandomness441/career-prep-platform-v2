You're dropped into an existing connector class. The constructor and a couple of getters
are already written. One method is missing: `syncTable`. Everything else in the class you
can treat as done and correct.

Implement the one missing method:

```java
void syncTable(String table)
```

Use `driver.queryUpdatedSince(table, cursor)` to fetch every row in `table` whose
`updated_at` is greater than the connector's current `cursor`. Append each row returned
to `synced`, in the order the driver returns them, and advance `cursor` to the maximum
`updated_at` among the rows just fetched.

### Requirements

- If nothing comes back from the driver, `synced` and `cursor` are both left exactly as they were -- don't reset the cursor, don't add empty rows.
- Calling `syncTable` again later, after more rows have shown up in the source, must only fetch and append the rows that are new since the last call -- it picks up from `cursor`, not from the beginning.
- `synced` accumulates across every call -- don't clear it, don't replace it.
