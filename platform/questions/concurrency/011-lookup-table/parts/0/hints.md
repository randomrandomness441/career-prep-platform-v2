One mutex over the whole table cannot express "I am only reading" — `std::mutex` has no
shared mode, so readers take turns even though they cannot interfere with each other. And
one lock for all keys makes two lookups of completely unrelated keys wait for each other.
Split the data so independent keys live under independent locks, and pick a lock type that
lets readers hold it together. Remember what `value_for` must hand back: a copy, made
while the protection is still held.
---
Each bucket owns a `std::list<std::pair<Key, Value>>` and its own `mutable
std::shared_mutex`; `hasher(key) % num_buckets` picks the bucket. A lookup takes a
`std::shared_lock` (readers share), finds the entry with a linear `std::find_if`, and
copies the value out before the lock releases. Insert/update/erase take a
`std::unique_lock` on that one bucket and no other. Store the buckets as
`std::vector<std::unique_ptr<bucket>>`, filled once in the constructor — the vector never
resizes, so picking a bucket needs no lock at all.
---
```cpp
Value value_for(const Key& key, const Value& default_value) const {
    std::shared_lock<std::shared_mutex> lock(mutex);          // readers share the bucket
    const auto found = std::find_if(data.begin(), data.end(),
        [&](const bucket_value& e) { return e.first == key; });
    return found == data.end() ? default_value : found->second;   // copy, under the lock
}

void add_or_update_mapping(const Key& key, const Value& value) {
    std::unique_lock<std::shared_mutex> lock(mutex);          // this writer is alone
    const auto found = find_entry_for(key);
    if (found == data.end()) data.push_back(bucket_value(key, value));
    else                     found->second = value;           // update in place
}
```
`remove_mapping` is the same shape: `unique_lock`, find, `erase` if found. Idempotence
falls out of "erase only what you found".
