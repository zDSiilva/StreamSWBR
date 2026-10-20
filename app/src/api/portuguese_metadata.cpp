#include "api/portuguese_metadata.hpp"
#include "api/http.hpp"
#include "activity/stream_preferences.hpp"
#include <borealis/core/thread.hpp>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <ctime>
#include <chrono>

namespace portuguese_metadata {
namespace {
using Json = nlohmann::json;
std::string root;
std::mutex cacheMutex, requestMutex;
std::map<std::string, Json> memory;
std::string text(const Json& j, const char* key) {
    auto i = j.find(key);
    return i != j.end() && i->is_string() ? i->get<std::string>() : "";
}
bool imdb(const std::string& id) {
    return id.size() > 2 && id.substr(0, 2) == "tt" &&
        id.find_first_not_of("0123456789", 2) == std::string::npos;
}
Json cached(const std::string& key) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    Json record;
    auto it = memory.find(key);
    if (it != memory.end()) record = it->second;
    else {
        try { std::ifstream in(root + "/" + key + ".json"); if (in) in >> record; } catch (...) {}
    }
    if (!record.is_object() || !record.contains("until") || !record["until"].is_number_integer() ||
        record["until"].get<int64_t>() < std::time(nullptr) || !record.contains("data") || !record["data"].is_object()) return nullptr;
    memory[key] = record;
    return record.value("data", Json::object());
}
Json fetch(const std::string& key, const std::string& path) {
    auto token = stream_preferences::get().tmdbToken;
    if (token.empty()) return Json::object();
    Json hit = cached(key);
    if (!hit.is_null()) return hit;
    // Deduplicate cache misses and limit TMDB traffic independently of carousel loading.
    std::lock_guard<std::mutex> network(requestMutex);
    hit = cached(key);
    if (!hit.is_null()) return hit;
    Json data = Json::object();
    try {
        const std::string sep = path.find('?') == std::string::npos ? "?" : "&";
        auto body = HTTP::get("https://api.themoviedb.org/3/" + path + sep + "language=pt-BR",
            HTTP::Header{"Authorization: Bearer " + token, "Accept: application/json"}, HTTP::Timeout{4000});
        data = Json::parse(body);
        if (!data.is_object() || data.contains("success")) data = Json::object();
    } catch (...) { /* Metadata failure must never prevent playback. */ }
    Json record{{"until", int64_t(std::time(nullptr)) + (data.empty() ? 300 : 604800)}, {"data", data}};
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        if (memory.size() >= 512) memory.clear();
        memory[key] = record;
        try { std::ofstream out(root + "/" + key + ".json"); if (out) out << record.dump(); } catch (...) {}
    }
    return data;
}
Json find(const std::string& id, const std::string& type) {
    if (!imdb(id) || (type != "movie" && type != "series")) return Json::object();
    auto result = fetch("find-" + id, "find/" + id + "?external_source=imdb_id");
    auto it = result.find(type == "movie" ? "movie_results" : "tv_results");
    return it != result.end() && it->is_array() && !it->empty() ? (*it)[0] : Json::object();
}
std::string name(const Json& j) { auto n = text(j, "title"); return n.empty() ? text(j, "name") : n; }

class Queue {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::function<void()>> work;
    bool stopping = false;
    std::thread worker;
public:
    Queue() : worker([this]() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [this]() { return stopping || !work.empty(); });
                if (stopping) return;
                job = std::move(work.front()); work.pop_front();
            }
            try { job(); } catch (...) {}
        }
    }) {}
    ~Queue() { { std::lock_guard<std::mutex> lock(mutex); stopping = true; } wake.notify_all(); worker.join(); }
    bool push(std::function<void()> job, bool priority = false) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (work.size() >= 128 && !priority) return false;
            if (priority) work.push_front(std::move(job)); else work.push_back(std::move(job));
        }
        wake.notify_one();
        return true;
    }
};
Queue& queue() { static Queue q; return q; }
}
void configure(const std::string& directory) {
    root = directory + "/metadata-pt";
    std::error_code error;
    std::filesystem::create_directories(root, error);
}
std::string cachedTitle(const std::string& id, const std::string& type, const std::string& fallback) {
    if (stream_preferences::get().tmdbToken.empty() || !imdb(id)) return fallback;
    auto result = cached("find-" + id);
    if (result.is_null()) return fallback;
    auto i = result.find(type == "movie" ? "movie_results" : "tv_results");
    if (i == result.end() || !i->is_array() || i->empty()) return fallback;
    auto n = name((*i)[0]); return n.empty() ? fallback : n;
}
void title(const std::string& id, const std::string& type, std::function<void(std::string)> callback) {
    if (stream_preferences::get().tmdbToken.empty() || !imdb(id)) { callback(""); return; }
    auto hit = cachedTitle(id, type, "");
    if (!hit.empty()) { callback(hit); return; }
    if (!queue().push([id, type, callback]() {
        auto n = name(find(id, type));
        brls::sync([callback, n]() { callback(n); });
    })) callback("");
}
void localizeDetail(Json& response) {
    if (!response.contains("meta") || !response["meta"].is_object()) return;
    auto& meta = response["meta"];
    static const std::map<std::string, std::string> genres = {{"Action", "Ação"}, {"Adventure", "Aventura"},
        {"Animation", "Animação"}, {"Biography", "Biografia"}, {"Comedy", "Comédia"}, {"Crime", "Crime"},
        {"Documentary", "Documentário"}, {"Drama", "Drama"}, {"Family", "Família"}, {"Fantasy", "Fantasia"},
        {"History", "História"}, {"Horror", "Terror"}, {"Music", "Música"}, {"Musical", "Musical"},
        {"Mystery", "Mistério"}, {"Romance", "Romance"}, {"Sci-Fi", "Ficção científica"}, {"Sport", "Esporte"},
        {"Thriller", "Suspense"}, {"War", "Guerra"}, {"Western", "Faroeste"}};
    for (const char* field : {"genres", "genre"}) {
        auto it = meta.find(field);
        if (it != meta.end() && it->is_array()) for (auto& value : *it) {
            if (!value.is_string()) continue;
            auto translated = genres.find(value.get<std::string>());
            if (translated != genres.end()) value = translated->second;
        }
    }
    const std::string id = text(meta, "id"), type = text(meta, "type");
    auto found = find(id, type);
    if (!found.contains("id") || !found["id"].is_number_integer()) {
        meta["description"] = stream_preferences::get().tmdbToken.empty() ?
            "Sinopse ainda não ativada. Abra Configurações → Informações em português e siga o tutorial do TMDB." : "Sinopse em português indisponível.";
        return;
    }
    auto prefix = std::string(type == "movie" ? "movie/" : "tv/") + std::to_string(found["id"].get<int>());
    auto details = fetch("detail-" + type + "-" + id, prefix);
    if (details.empty()) details = found;
    auto n = name(details); if (!n.empty()) meta["name"] = n;
    auto overview = text(details, "overview");
    meta["description"] = overview.empty() ? "Sinopse em português indisponível." : overview;
    if (details.contains("genres") && details["genres"].is_array()) {
        meta["genres"] = Json::array();
        for (const auto& genre : details["genres"]) {
            auto g = text(genre, "name"); if (!g.empty()) meta["genres"].push_back(g);
        }
    }
}
void season(const std::string& id, int number, std::function<void(Json)> callback) {
    queue().push([id, number, callback]() {
        auto show = find(id, "series");
        Json result = Json::object();
        if (number >= 0 && show.contains("id") && show["id"].is_number_integer())
            result = fetch("season-" + id + "-" + std::to_string(number),
                "tv/" + std::to_string(show["id"].get<int>()) + "/season/" + std::to_string(number));
        brls::sync([callback, result]() { callback(result); });
    }, true);
}
void search(const std::string& query, std::function<void(Json)> callback) {
    queue().push([query, callback]() {
        std::string encoded;
        for (unsigned char c : query) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~') encoded += char(c);
            else { encoded += '%'; encoded += "0123456789ABCDEF"[c >> 4]; encoded += "0123456789ABCDEF"[c & 15]; }
        }
        Json output{{"metas", Json::array()}};
        auto results = fetch("search-" + std::to_string(std::hash<std::string>{}(query)),
            "search/multi?include_adult=false&query=" + encoded);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        if (results.contains("results") && results["results"].is_array()) {
            for (const auto& item : results["results"]) {
                if (std::chrono::steady_clock::now() > deadline) break;
                auto type = text(item, "media_type");
                if ((type != "movie" && type != "tv") || !item.contains("id") || !item["id"].is_number_integer()) continue;
                auto numeric = std::to_string(item["id"].get<int>());
                auto external = fetch("external-" + type + "-" + numeric, type + "/" + numeric + "/external_ids");
                auto id = text(external, "imdb_id");
                if (!imdb(id)) continue;
                auto date = text(item, type == "movie" ? "release_date" : "first_air_date");
                auto poster = text(item, "poster_path");
                output["metas"].push_back({{"id", id}, {"type", type == "movie" ? "movie" : "series"},
                    {"name", name(item)}, {"year", date.substr(0, 4)},
                    {"poster", poster.empty() ? "" : "https://image.tmdb.org/t/p/w500" + poster}});
            }
        }
        brls::sync([callback, output]() { callback(output); });
    }, true);
}
void clearCache() {
    std::lock_guard<std::mutex> network(requestMutex);
    std::lock_guard<std::mutex> lock(cacheMutex);
    memory.clear();
    std::error_code error;
    for (std::filesystem::directory_iterator i(root, error), end; !error && i != end; i.increment(error))
        if (i->path().extension() == ".json") std::filesystem::remove(i->path(), error);
}
}
