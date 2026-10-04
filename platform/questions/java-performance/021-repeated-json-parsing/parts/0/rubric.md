A good answer covers:

- **Why the gap is bigger than regex's.** `Pattern.compile` parses a regex string into
  a state machine — real work, but bounded and self-contained. `ObjectMapper`
  construction does more: it scans the target class via reflection (finding fields,
  getters, constructors, annotations), builds a serializer and deserializer strategy for
  it, and sets up multiple internal caches — several distinct pieces of setup work
  rather than one parsing pass, which plausibly explains a larger constant-factor gap. A
  good answer names reflection-based class scanning specifically as the extra cost
  regex compilation doesn't have.
- **The real fan-out arithmetic.** Parsing once and distributing the already-parsed
  result costs one parse, however expensive that is, regardless of client count.
  Re-parsing per client turns that one cost into 10,000 copies of it — the actual
  multiplier is the *client count*, not the payload size alone, and it compounds with
  message frequency (a 2MB payload arriving repeatedly at scale makes this multiply
  again on top of the per-client multiplier). A good answer states the multiplication
  explicitly (parse-once-cost × clients, versus parse-once-cost × 1), not just "it's
  more expensive."
- **When streaming is actually worth it.** When document size is large enough, or
  request volume high enough, that the tree model's 2-14x memory multiplier becomes a
  real capacity or GC-pressure problem — not by default for small, occasional payloads,
  where the tree model's easier-to-read code is worth its overhead. A good answer frames
  this as a real tradeoff triggered by scale (document size or throughput), not "always
  use streaming" or "tree model is fine, don't worry about it."

NEEDS_WORK if the answer treats this as identical in mechanism to the regex case with
no distinguishing explanation, or recommends streaming or tree-model unconditionally
without naming the scale threshold that should drive the choice.

## Code

**Inefficient — a fresh ObjectMapper built on every call:**
```java
Person parse(String json) throws Exception {
    return new ObjectMapper().readValue(json, Person.class);   // rebuilds caches every call
}
```

**Correct — build once, reuse the shared, thread-safe instance:**
```java
private static final ObjectMapper MAPPER = new ObjectMapper();   // configured once, at startup

Person parse(String json) throws Exception {
    return MAPPER.readValue(json, Person.class);
}
```

**Alternative — parse once, fan out the result instead of re-parsing per recipient:**
```java
Person parsed = MAPPER.readValue(json, Person.class);   // once
for (Client c : connectedClients) c.send(parsed);        // not: c.send(MAPPER.readValue(json, ...))
```

