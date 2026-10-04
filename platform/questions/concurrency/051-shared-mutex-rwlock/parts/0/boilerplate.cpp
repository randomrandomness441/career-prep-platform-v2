#include <shared_mutex>
#include <string>
#include <unordered_map>

// A config store. Any number of readers may call get() at the same time.
// set() mutates the store and must never run concurrently with another
// set(), or with any get().
class ConfigStore {
    mutable std::shared_mutex m_;
    std::unordered_map<std::string, std::string> data_;

public:
    void set(std::string key, std::string value) {
        // TODO: implement
        (void)key;
        (void)value;
    }

    std::string get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lk(m_);
        auto it = data_.find(key);
        return it == data_.end() ? std::string() : it->second;
    }
};
