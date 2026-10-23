#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <fstream>
#include <mutex>
namespace watched {
class Store {
    std::set<std::string> ids;
    std::string path;
    mutable std::mutex mutex;
public:
    void load(const std::string& directory) {
        std::lock_guard<std::mutex> lock(mutex); path=directory+"/watched-episodes.json"; ids.clear();
        try { std::ifstream in(path); nlohmann::json data; if (in) in>>data;
            if (data.is_array()) for (const auto& id:data)
                if (id.is_string() && !id.get<std::string>().empty()) ids.insert(id.get<std::string>());
        } catch (...) {}
    }
    bool contains(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mutex); return ids.count(id)!=0;
    }
    bool mark(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex);
        if (id.empty() || path.empty() || !ids.insert(id).second) return false;
        try { std::ofstream out(path); if (out) out<<nlohmann::json(ids).dump(); } catch (...) {}
        return true;
    }
};
inline Store& store() { static Store data; return data; }
}
