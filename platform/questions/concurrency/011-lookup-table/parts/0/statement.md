# A Thread-Safe Lookup Table

## ELI5: a card catalog with many small drawers, not one giant one

An old library card catalog could be built two ways. Way one: a single enormous drawer
holding every card in the whole library, with one line of people waiting to open it,
whether you want "Adventure novels" or "Zoology," you're standing in the same line as
everyone else, one person at a time. Way two: dozens of smaller drawers, each holding
only cards starting with certain letters. Now someone looking up "Adventure" and someone
looking up "Zoology" never even meet, they're at completely different drawers. And
several people browsing the *same* drawer, just reading, don't need to take turns either
, only actually updating a card needs the drawer to itself for a moment.

Wrapping the whole catalog in one lock is way one: safe, and exactly why the library
doesn't scale. This question asks for way two, the version a senior reviewer expects.

## What you're actually building

A shared map that many threads read while a few update it, the shape of every cache,
config store and routing table.

```cpp
template <typename Key, typename Value>
class threadsafe_lookup_table {
public:
    explicit threadsafe_lookup_table(unsigned num_buckets = 19);

    threadsafe_lookup_table(const threadsafe_lookup_table&) = delete;
    threadsafe_lookup_table& operator=(const threadsafe_lookup_table&) = delete;

    Value value_for(const Key& key, const Value& default_value = Value()) const;
    void add_or_update_mapping(const Key& key, const Value& value);
    void remove_mapping(const Key& key);
};
```

## Requirements

1. **Readers run concurrently.** A lookup takes nothing away from another lookup, so two
 threads reading the same bucket (drawer) at the same time must not queue behind each
 other.
2. **Unrelated keys do not contend.** Two threads looking up two different keys should
 never meet on the same lock, different drawers entirely.
3. `value_for` returns `Value` **by value**, a copy made while the bucket is protected.
 On a miss it returns the caller's `default_value`; the one-argument overload returns
 `Value()`.
4. `add_or_update_mapping` updates an existing entry in place. Inserting a second entry
 for the same key is wrong, a later lookup may find the stale one. `remove_mapping` of
 a key that is not there must be harmless.
5. Correct with any bucket count: 19, 257, and 1, where every key collides into the same
 bucket and the structure degenerates to one lock (back to the single giant drawer).
 The tests run all three, plus `int` and `std::string` keys.

## Why the constraints exist

- **The bucket count is fixed at construction and never changes.** Design for that; do
 not rehash. The number of drawers is decided once, when the catalog is built.
- **Nothing a caller holds after an operation returns may point into the table.** A
 caller shouldn't walk away holding a reference into a drawer that could change under
 them the moment they let go.
