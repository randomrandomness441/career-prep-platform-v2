A good answer covers:

- **Names the comparison semantics up front, doesn't assume byte-identical.** A processing
  step that's supposed to transform the file (filter rows, rename a column, reformat a
  date) makes byte-for-byte comparison meaningless -- it will always report a difference,
  correctly transformed or not. A good answer picks a comparison mode explicitly: exact
  (nothing should have changed), row-set (same records, any order), or schema-aware
  (records should match after applying the KNOWN expected transformation, and anything
  beyond that is flagged).
- **A row-level comparison key, not positional matching.** If rows can be reordered, you
  can't compare line 5 of file A to line 5 of file B -- you need a key (a declared primary
  key column, or a hash of the row's content if no natural key exists) to match up "the
  same logical record" across both files regardless of position. This is precisely the
  reconciler-by-composite-key pattern from this pack's coding questions, at the file level
  instead of two database snapshots -- a good answer makes that connection explicitly.
- **Bounded memory for large files.** Doesn't load both files whole into memory and diff
  them as in-memory structures. Streams both files, computing a per-row hash (content hash,
  not the whole row) and comparing hash-by-key in a single pass (or two sorted passes, like
  a merge-join) rather than materializing everything. For files too large even for a
  hash-set of every key, a good answer proposes a streaming external sort or a bucketed/
  sampled comparison with a stated false-negative tradeoff.
- **Reports differences usefully, not just true/false.** Categorizes into added (key in
  after-file, not before), removed (key in before-file, not after), and changed (key in
  both, content hash differs) -- the same three buckets as the reconciler's insert/update/
  delete split. "Files differ" with no further detail is a bad design for this problem,
  since the whole point is surfacing what a processing step actually did.
- **Handles expected, allowed differences without false-alarming.** Names concrete
  examples: row reorder (solved by the key-based matching itself, not positional
  comparison), whitespace/encoding normalization, a timestamp reformatted to a different
  but equivalent representation. A good answer either normalizes known-benign
  transformations before hashing, or takes a declared allowlist of "these columns are
  expected to change, exclude them from the comparison" -- and explicitly separates that
  from real corruption.
- **Ties it to why this matters for a data pipeline specifically.** A comparator like this
  is how you'd validate a schema migration, a backfill, or a reprocessing job actually
  preserved data correctly -- a good answer connects the design to that real use case
  rather than treating it as an abstract diffing exercise.

NEEDS_WORK if the answer assumes byte-identical comparison without ever questioning
whether that's the right semantics for a "before vs after processing" comparison, if rows
are matched positionally with no discussion of what happens when order isn't guaranteed,
or if the answer never addresses files too large to hold in memory.

## Code

**Byte-for-byte diff -- flags every legitimate transformation as corruption:**
```python
def compare(file_a, file_b):
    return file_a.read() == file_b.read()
    # a column rename, a reordered row, or a reformatted timestamp all report
    # "files differ" even when the processing step did exactly what it was supposed to
```

**Key-based, streaming, categorized comparison:**
```python
def hash_row(row, key_fn, exclude_cols=()):
    normalized = {k: v for k, v in row.items() if k not in exclude_cols}
    return key_fn(row), hash(frozenset(normalized.items()))

before_hashes = {}
for row in stream(file_before):
    key, h = hash_row(row, key_fn=lambda r: r["id"])
    before_hashes[key] = h   # bounded by number of distinct keys, not file size

added, changed, removed = [], [], set(before_hashes)
for row in stream(file_after):
    key, h = hash_row(row, key_fn=lambda r: r["id"])
    removed.discard(key)
    if key not in before_hashes:
        added.append(key)
    elif before_hashes[key] != h:
        changed.append(key)
# removed now holds exactly the keys present in "before" but missing from "after"
```
