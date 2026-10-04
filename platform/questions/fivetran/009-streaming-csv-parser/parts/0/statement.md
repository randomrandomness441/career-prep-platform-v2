A connector reads CSV exports that can be gigabytes in size. Reading the whole file into a
`List<String>` before parsing it would work on a sample file and blow up memory on a real
one, so parsing has to be line-at-a-time against a source that hands you one line per call.

Implement:

```java
static ParseResult parse(LineSource source, MalformedPolicy policy)
```

`source.next()` returns the next raw line, or `null` once the input is exhausted -- call it
once per line, never build a list of "all the lines" up front. The first line is the header
(comma-separated column names). Every line after that is a data row, split on commas; a row
is malformed if it doesn't have exactly as many fields as the header.

`MalformedPolicy` is `SKIP_AND_COUNT` or `FAIL_FAST`:

- `SKIP_AND_COUNT`: skip the malformed row, keep going, and count how many were skipped.
- `FAIL_FAST`: throw `IllegalStateException` the moment a malformed row is hit. Don't call `source.next()` again after that -- stop reading immediately.

Return a `ParseResult` with the successfully parsed rows (as `column name -> value` maps,
in the order they appeared) and the count of malformed rows skipped (`0` under
`FAIL_FAST`, since it never gets the chance to skip more than one).

### Constraints

- Call `source.next()` exactly once per line of input plus one final call that returns `null` -- no re-reading, no calling it more times than there are lines.
- A malformed row under `SKIP_AND_COUNT` doesn't stop parsing -- rows after it still get processed normally.
