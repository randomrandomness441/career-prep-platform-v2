Before a schema migration loads a batch of tables, it has to load them in an order where
every table's foreign-key targets already exist. `Collections.sort` with the right
`Comparator<Table>` gets you that order for free -- as long as the comparator actually
understands the whole dependency graph, not just the two tables it's currently looking at.

Implement a class `Solution implements Comparator<Table>`:

```java
Solution(List<Table> allTables)
public int compare(Table a, Table b)
```

The constructor sees every table up front. `Table` has `name` and `dependsOn` (the names
of tables it has a foreign key into -- can be more than one, can be empty).

### Requirements

- After sorting `allTables` with this comparator, every table must come after every other table in `allTables` that it (directly or indirectly) depends on. A comparator that only checks whether `a` and `b` have a direct foreign key between them isn't enough -- a table two levels down its dependency chain from another has no direct edge to it at all, but still needs to sort after it.
- A name in `dependsOn` that isn't among `allTables` imposes no ordering constraint -- that table already exists in the destination.
- If `allTables` contains a real dependency cycle, throw `IllegalArgumentException` -- from the constructor, before anyone even calls `compare`. Don't let `compare` return inconsistent results for a graph that can't actually be ordered.
