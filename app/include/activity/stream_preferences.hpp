#pragma once
#include <nlohmann/json.hpp>
#include <fstream>
#include <mutex>
#include <string>

namespace stream_preferences {
struct Values {
    std::string tmdbToken;
    std::string preferredServer;
    std::string audio = "por,pt,pt-BR,eng,en";
    std::string subtitles = "por,pt,pt-BR";
};
inline std::mutex mutex;
inline Values values;
inline std::string directory;
inline Values get() { std::lock_guard<std::mutex> lock(mutex); return values; }
inline void load(const std::string& dir) {
    std::lock_guard<std::mutex> lock(mutex);
    directory = dir;
    values=Values{};
    try {
        std::ifstream in(dir + "/preferences-pt.json");
        if (!in) return;
        nlohmann::json j; in >> j; in.close();
        values.tmdbToken = j.value("tmdbToken", "");
        values.preferredServer = j.value("preferredServer", "");
        values.audio = j.value("audio", "por,pt,pt-BR,eng,en");
        values.subtitles = j.value("subtitles", "por,pt,pt-BR");
        if (j.value("sourceOrderVersion",0)<1) {
            values.preferredServer.clear();
            j["preferredServer"]=""; j["sourceOrderVersion"]=1;
            std::ofstream out(dir+"/preferences-pt.json"); if (out) out<<j.dump(2);
        }
    } catch (...) { /* Keep defaults if the configuration is invalid. */ }
}
inline bool save(const Values& next) {
    std::lock_guard<std::mutex> lock(mutex);
    const std::string path = directory + "/preferences-pt.json";
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << nlohmann::json{{"sourceOrderVersion",1},{"tmdbToken", next.tmdbToken}, {"preferredServer", next.preferredServer},
        {"audio", next.audio}, {"subtitles", next.subtitles}}.dump(2);
    out.close();
    if (out.fail()) return false;
    values = next;
    return true;
}
inline void importFile(const std::string& dir) {
    for (const auto& path : {std::string("sdmc:/switch/streamfin-addon.txt"), dir + "/addon.txt"}) {
        std::ifstream in(path);
        if (!in) continue;
        auto next = get();
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("tmdb_token=", 0) != 0) continue;
            auto token = line.substr(11);
            const auto end = token.find_last_not_of(" \t\r\n");
            if (end == std::string::npos) continue;
            token.resize(end + 1);
            if (token.empty() || token.find_first_of(" \t\r\n") != std::string::npos) continue;
            if (token != next.tmdbToken) { next.tmdbToken = token; save(next); }
        }
        return;
    }
}
}
