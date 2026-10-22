#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <ctime>
namespace meta_cache {
inline std::string root;
inline std::mutex mutex;
inline bool eligible(const std::string& url) {
    return url.rfind("https://v3-cinemeta.strem.io/meta/",0)==0 || url.rfind("https://anime-kitsu.strem.fun/meta/",0)==0;
}
inline std::string path(const std::string& url) {
    uint64_t hash=14695981039346656037ULL;
    for (unsigned char c:url) { hash^=c; hash*=1099511628211ULL; }
    return root+"/"+std::to_string(hash)+".json";
}
inline void configure(const std::string& directory) {
    std::lock_guard<std::mutex> lock(mutex); root=directory+"/episode-metadata";
    std::error_code error; std::filesystem::create_directories(root,error);
}
inline nlohmann::json read(const std::string& url,bool stale=false) {
    std::lock_guard<std::mutex> lock(mutex);
    if (root.empty() || !eligible(url)) return nullptr;
    try {
        std::ifstream in(path(url)); if (!in) return nullptr;
        nlohmann::json j; in>>j;
        if (!j.contains("data") || !j["data"].contains("meta") || !j["data"]["meta"].is_object() || j["data"]["meta"].empty()) return nullptr;
        if (!stale && j.value("until",int64_t(0))<std::time(nullptr)) return nullptr;
        return j["data"];
    } catch (...) { return nullptr; }
}
inline void write(const std::string& url,const nlohmann::json& data) {
    std::lock_guard<std::mutex> lock(mutex);
    if (root.empty() || !eligible(url) || !data.contains("meta") || !data["meta"].is_object() || data["meta"].empty()) return;
    try { std::ofstream out(path(url)); if (out) out<<nlohmann::json{{"until",int64_t(std::time(nullptr))+21600},{"data",data}}; } catch (...) {}
}
}
