#include <algorithm>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <shared_mutex>
#include <utility>
#include <vector>

template <typename Key, typename Value>
class threadsafe_lookup_table {
private:
    class bucket_type {
    public:
        using bucket_value = std::pair<Key, Value>;
        using bucket_data = std::list<bucket_value>;

        // One lock per bucket. Two threads working on different buckets never
        // meet; two readers on the same bucket share it.
        mutable std::shared_mutex mutex;
        bucket_data data;

        typename bucket_data::iterator find_entry_for(const Key& key) {
            return std::find_if(data.begin(), data.end(),
                                [&](const bucket_value& item) { return item.first == key; });
        }
        typename bucket_data::const_iterator find_entry_for(const Key& key) const {
            return std::find_if(data.begin(), data.end(),
                                [&](const bucket_value& item) { return item.first == key; });
        }

        Value value_for(const Key& key, const Value& default_value) const {
            std::shared_lock<std::shared_mutex> lock(mutex);   // shared: readers run together
            const auto found = find_entry_for(key);
            // The copy is made here, while the lock is still held. Returning
            // found->second by reference would hand the caller a pointer into a
            // list that another thread may erase one instruction later.
            return found == data.end() ? default_value : found->second;
        }

        void add_or_update_mapping(const Key& key, const Value& value) {
            std::unique_lock<std::shared_mutex> lock(mutex);   // exclusive: writer
            const auto found = find_entry_for(key);
            if (found == data.end()) {
                data.push_back(bucket_value(key, value));
            } else {
                found->second = value;                          // update, not a second entry
            }
        }

        void remove_mapping(const Key& key) {
            std::unique_lock<std::shared_mutex> lock(mutex);
            const auto found = find_entry_for(key);
            if (found != data.end()) data.erase(found);
        }
    };

    // Fixed for the lifetime of the table. Nothing ever resizes this vector, so
    // no lock is needed to read it -- which is exactly what makes get_bucket()
    // free, and what makes the whole design work.
    std::vector<std::unique_ptr<bucket_type>> buckets;
    std::hash<Key> hasher;

    bucket_type& get_bucket(const Key& key) const {
        const std::size_t index = hasher(key) % buckets.size();
        return *buckets[index];
    }

public:
    explicit threadsafe_lookup_table(unsigned num_buckets = 19)
        : buckets(num_buckets == 0 ? 1 : num_buckets) {
        for (auto& bucket : buckets) bucket.reset(new bucket_type);
    }

    threadsafe_lookup_table(const threadsafe_lookup_table&) = delete;
    threadsafe_lookup_table& operator=(const threadsafe_lookup_table&) = delete;

    Value value_for(const Key& key, const Value& default_value = Value()) const {
        return get_bucket(key).value_for(key, default_value);
    }

    void add_or_update_mapping(const Key& key, const Value& value) {
        get_bucket(key).add_or_update_mapping(key, value);
    }

    void remove_mapping(const Key& key) {
        get_bucket(key).remove_mapping(key);
    }
};
