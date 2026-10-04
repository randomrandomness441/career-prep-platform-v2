## The problem

Design a system that keeps a destination SQL database continuously up to date with a
source SQL database -- tables in the source need to show up, and stay current, in the
destination, without re-copying the whole source on every run.

Cover, in your answer:

- Whether you're building ETL (transform before loading) or ELT (load raw, transform in
  the destination) and why that choice fits this problem.
- How you extract incrementally instead of doing a full table scan every run, and how you
  detect rows that changed, rows that are new, and rows that were deleted in the source.
- What "up to date" actually means here -- how far behind the destination is allowed to
  get, and what happens if the source is a live production database under write load
  while you're reading from it.
- How a table with a foreign key into another table gets loaded without violating the
  constraint, when both tables have new rows in the same run.
- What happens when the source's schema changes (a column added, a column's type changed)
  mid-flight.

There's no code to write here. Answer in plain writing, the way you'd talk it through on a
whiteboard.
