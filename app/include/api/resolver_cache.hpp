#pragma once
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <functional>
namespace resolver_cache {
// Only DNS resolution is serialized, never the HTTP transfer. Failed lookups are not cached.
class Cache {
    struct Entry { std::string address; std::chrono::steady_clock::time_point until; };
    std::mutex mutex;
    std::map<std::string,Entry> entries;
public:
    std::string get(const std::string& host,const std::function<std::string()>& resolve) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it=entries.find(host); auto now=std::chrono::steady_clock::now();
        if (it!=entries.end() && it->second.until>now) return it->second.address;
        auto address=resolve();
        if (!address.empty()) {
            if (entries.size()>=64) entries.clear();
            entries[host]={address,now+std::chrono::seconds(60)};
        }
        return address;
    }
    void invalidate(const std::string& host) { std::lock_guard<std::mutex> lock(mutex); entries.erase(host); }
};
}
