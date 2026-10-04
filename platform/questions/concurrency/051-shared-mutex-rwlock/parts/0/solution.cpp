#include <shared_mutex>
#include <string>
#include <unordered_map>

// Same store, one change: set() takes a unique_lock. Readers still share
// std::shared_lock and can run concurrently with each other; a writer gets
// exclusive access against every reader AND every other writer.
class ConfigStore {
    mutable std::shared_mutex m_;
    std::unordered_map<std::string, std::string> data_;

public:
    void set(std::string key, std::string value) {
        std::unique_lock<std::shared_mutex> lk(m_);
        data_[std::move(key)] = std::move(value);
    }

    std::string get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lk(m_);
        auto it = data_.find(key);
        return it == data_.end() ? std::string() : it->second;
    }
};
