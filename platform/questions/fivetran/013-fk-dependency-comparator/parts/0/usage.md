Your submission is a single class, no `public` modifier needed (the harness compiles it
alongside its own `Tests.java`, which also defines `Table`):

```java
class Table {
    final String name;
    final List<String> dependsOn;
    Table(String name, List<String> dependsOn) { this.name = name; this.dependsOn = dependsOn; }
}

class Solution implements Comparator<Table> {
    Solution(List<Table> allTables) { ... }
    public int compare(Table a, Table b) { ... }
}
```

A caller builds the comparator once from the full table list, then sorts with it:

```java
List<Table> tables = List.of(
    new Table("order_items", List.of("orders", "products")),
    new Table("orders", List.of("users")),
    new Table("products", List.of()),
    new Table("users", List.of()));

tables.sort(new Solution(tables));
// users and products (in either order) come first, then orders, then order_items last.
```
