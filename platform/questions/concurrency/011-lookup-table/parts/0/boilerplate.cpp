#include <cstddef>
#include <functional>
#include <map>
#include <mutex>

// A key/value table, safe for any number of threads to read and write at
// once. Two threads reading, even the same key, should never have to wait on
// each other -- only a write needs exclusivity.
template <typename Key, typename Value>
class threadsafe_lookup_table {
public:
    explicit threadsafe_lookup_table(unsigned num_buckets = 19) {
        // TODO: implement
        (void)num_buckets;
    }

    threadsafe_lookup_table(const threadsafe_lookup_table&) = delete;
    threadsafe_lookup_table& operator=(const threadsafe_lookup_table&) = delete;

    Value value_for(const Key& key, const Value& default_value = Value()) const {
        // TODO: implement
        (void)key;
        return default_value;
    }

    void add_or_update_mapping(const Key& key, const Value& value) {
        // TODO: implement
        (void)key; (void)value;
    }

    void remove_mapping(const Key& key) {
        // TODO: implement
        (void)key;
    }
};
