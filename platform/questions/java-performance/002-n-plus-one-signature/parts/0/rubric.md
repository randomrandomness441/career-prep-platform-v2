A good answer covers:

- **The arithmetic.** 201 total queries (1 + 200). 200 x 3ms = 600ms spent purely in
  sequential `line_items` round trips, on top of the 4ms `orders` query — the 200 small
  queries dominate total latency even though each one individually looks fast and cheap.
- **The visual signature, named explicitly.** A long sequence of near-identical, narrow,
  same-shaped spans/frames repeated back to back — "machine-gun fire" is the point: you
  don't need to read a single query's SQL to suspect N+1, the repetition and uniformity
  of the pattern is the tell. Contrast this explicitly with question 004's single wide
  box: N+1 is recognized by *count and repetition*, not by *width*.
- **The real cost of eager-loading everything.** Fetching every order's line items up
  front costs extra work (a bigger join, or extra batched queries) for every caller, even
  the ones that never touch `getLineItems()` — trading "N+1 for the callers who need it"
  for "1 bigger query for every caller, including the ones who didn't need the data at
  all." A good answer names this as a real tradeoff (over-fetching), not a free win, and
  ideally mentions that a middle ground exists (a separate, explicitly-eager query method
  used only by the callers that actually need line items).

NEEDS_WORK if the answer treats eager-loading as a strictly free fix with no downside.

## Code

**Inefficient — lazy load touched inside a loop, one query per order:**
```java
List<Order> orders = orderRepository.findAll();          // query #1
double total = 0;
for (Order o : orders) {
    total += o.getLineItems().stream()                    // fires query #2..#201
        .mapToDouble(LineItem::getPrice).sum();
}
```

**Correct — one query, eager-fetched, when every caller needs the line items:**
```java
@Query("SELECT o FROM Order o JOIN FETCH o.lineItems")
List<Order> findAllWithLineItems();                        // one query, one round trip
```

**Alternative — a separate eager method, keeping the lazy default for callers who don't need line items:**
```java
// Repository exposes both; lazy stays the default for callers that never touch line items
List<Order> findAll();                                      // lazy, unchanged
@Query("SELECT o FROM Order o JOIN FETCH o.lineItems")
List<Order> findAllWithLineItems();                          // eager, opt-in
```

