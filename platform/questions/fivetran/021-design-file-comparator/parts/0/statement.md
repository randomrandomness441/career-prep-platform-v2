## The problem

Design a system that compares a file before it goes through a processing step against the
same file after -- confirming the processing step did what it was supposed to (and nothing
more), and surfacing exactly what changed when it didn't.

This is the design/architecture version of the question: talk through the approach, don't
write the comparison code.

Cover, in your answer:

- What "compare" actually means here -- are the files expected to be byte-identical, row-
  identical-but-reordered, or identical-after-an-expected-transformation (a filter, a
  column rename, a type cast)? How your design handles each case differently.
- How you compare files too large to load entirely into memory.
- How you identify individual rows/records across the two files when they might not be in
  the same order -- what serves as the comparison key.
- How you report a mismatch usefully: not just "files differ" but which records were
  added, removed, or changed, and how.
- False positives: a processing step that's allowed to reorder rows, or normalize
  whitespace, or reformat a timestamp, shouldn't be reported as "data corrupted" just
  because the bytes don't match anymore.

There's no code to write here. Answer in plain writing, the way you'd talk it through on a
whiteboard.
