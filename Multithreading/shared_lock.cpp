#include <shared_mutex>
#include <string>
#include <unordered_map>

class ThreadSafeCache{
    mutable std::shared_mutex smtx_;
    std::unordered_map<std::string,std::string> cache_;

public:
    std::string read(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(smtx_);
        auto it = cache_.find(key);
        if(it == cache_.end()) return "";
        return it->second;
    }

    void write(const std::string key, std::string value){
        std::unique_lock<std::shared_mutex> lock(smtx_);
        cache_[key] = value;
    }

    bool erase(const std::string key){
        std::unique_lock<std::shared_mutex> lock(smtx_);
        auto it = cache_.find(key);
        if(it == cache_.end()) return false;
        cache_.erase(it);
        return true;
    }
};
