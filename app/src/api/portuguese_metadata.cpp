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
#include <set>

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
std::string imdbId(const Json& item) {
    for (const char* field : {"imdb_id", "id"}) { auto id = text(item,field); if (imdb(id)) return id; }
    auto links = item.find("links");
    if (links != item.end() && links->is_array()) for (const auto& link : *links) {
        if (text(link,"category") != "imdb") continue;
        auto url = text(link,"url");
        for (const auto& prefix : {std::string("https://imdb.com/title/"),std::string("https://www.imdb.com/title/")}) {
            if (url.rfind(prefix,0) != 0) continue;
            auto id = url.substr(prefix.size()); id = id.substr(0,id.find_first_of("/?#"));
            if (imdb(id)) return id;
        }
    }
    return "";
}
void rememberTitle(const std::string& id, const std::string& type, const std::string& title) {
    if (!imdb(id) || title.empty() || (type != "movie" && type != "series")) return;
    const auto key = "title-" + type + "-" + id;
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto existing = memory.find(key);
    if (existing != memory.end() && existing->second.value("data",Json::object()).value("name","") == title) return;
    Json record{{"until",int64_t(std::time(nullptr)) + 2592000},{"data",{{"name",title}}}};
    if (memory.size() >= 512) memory.clear();
    memory[key] = record;
    try { std::ofstream out(root + "/" + key + ".json"); if (out) out << record.dump(); } catch (...) {}
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
const std::string publicBase = "https://tmdb.elfhosted.com/%7B%22language%22%3A%22pt-BR%22%7D/";
std::chrono::steady_clock::time_point nextPublicRequest{}, retryPublicAfter{};
Json request(const std::string& key, const std::string& url, const HTTP::Header& headers) {
    Json hit = cached(key);
    if (!hit.is_null()) return hit;
    // Deduplicate cache misses and limit TMDB traffic independently of carousel loading.
    std::lock_guard<std::mutex> network(requestMutex);
    hit = cached(key);
    if (!hit.is_null()) return hit;
    const bool isPublic = url.rfind(publicBase,0) == 0;
    if (isPublic) {
        if (std::chrono::steady_clock::now() < retryPublicAfter) return Json::object();
        // Keep first-time carousel loading below 80 requests/minute per console.
        std::this_thread::sleep_until(nextPublicRequest);
        nextPublicRequest = std::chrono::steady_clock::now() + std::chrono::milliseconds(750);
    }
    Json data = Json::object();
    try {
        auto body = HTTP::get(url, headers, HTTP::Timeout{6000});
        data = Json::parse(body);
        if (!data.is_object() || data.contains("success")) data = Json::object();
    } catch (...) { /* Metadata failure must never prevent playback. */ }
    if (isPublic && data.empty()) retryPublicAfter = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    Json record{{"until", int64_t(std::time(nullptr)) + (data.empty() ? 300 : 604800)}, {"data", data}};
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        if (memory.size() >= 512) memory.clear();
        memory[key] = record;
        try { std::ofstream out(root + "/" + key + ".json"); if (out) out << record.dump(); } catch (...) {}
    }
    return data;
}
Json fetch(const std::string& key, const std::string& path) {
    auto token = stream_preferences::get().tmdbToken;
    if (token.empty()) return Json::object();
    const std::string sep = path.find('?') == std::string::npos ? "?" : "&";
    return request(key, "https://api.themoviedb.org/3/" + path + sep + "language=pt-BR",
        HTTP::Header{"Authorization: Bearer " + token, "Accept: application/json"});
}
std::string publicKey(const std::string& id, const std::string& type) { return "public-" + type + "-" + id; }
Json publicMeta(const std::string& id, const std::string& type) {
    if (!imdb(id) || (type != "movie" && type != "series")) return Json::object();
    auto data = request(publicKey(id,type), publicBase + "meta/" + type + "/" + id + ".json",
        HTTP::Header{"Accept: application/json"});
    auto m = data.find("meta");
    return m != data.end() && m->is_object() ? *m : Json::object();
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
    if (!imdb(id)) return fallback;
    if (stream_preferences::get().tmdbToken.empty()) {
        auto titleRecord = cached("title-" + type + "-" + id);
        auto titleName = text(titleRecord,"name"); if (!titleName.empty()) return titleName;
        auto record = cached(publicKey(id,type));
        if (!record.is_object() || !record.contains("meta") || !record["meta"].is_object()) return fallback;
        auto n = name(record["meta"]); return n.empty() ? fallback : n;
    }
    auto result = cached("find-" + id);
    if (result.is_null()) return fallback;
    auto i = result.find(type == "movie" ? "movie_results" : "tv_results");
    if (i == result.end() || !i->is_array() || i->empty()) return fallback;
    auto n = name((*i)[0]); return n.empty() ? fallback : n;
}
void title(const std::string& id, const std::string& type, std::function<void(std::string)> callback) {
    if (!imdb(id)) { callback(""); return; }
    auto hit = cachedTitle(id, type, "");
    if (!hit.empty()) { callback(hit); return; }
    if (!queue().push([id, type, callback]() {
        auto n = cachedTitle(id,type,"");
        if (n.empty()) n = name(stream_preferences::get().tmdbToken.empty() ? publicMeta(id,type) : find(id, type));
        if (stream_preferences::get().tmdbToken.empty()) rememberTitle(id,type,n);
        brls::sync([callback, n]() { callback(n); });
    })) callback("");
}
void localizeCatalog(Json& response) {
    if (!stream_preferences::get().tmdbToken.empty() || !response.contains("metas") || !response["metas"].is_array()) return;
    std::set<std::string> types;
    for (const auto& item : response["metas"]) {
        auto type = text(item,"type");
        if (imdb(text(item,"id")) && (type == "movie" || type == "series")) types.insert(type);
    }
    for (const auto& type : types) for (const auto& catalog : {std::string("tmdb.top"),std::string("tmdb.trending")}) {
        // One shared request provides up to 20 localized names instead of one per card.
        auto batch = request("batch-" + type + "-" + catalog,
            publicBase + "catalog/" + type + "/" + catalog + ".json", HTTP::Header{"Accept: application/json"});
        if (batch.contains("metas") && batch["metas"].is_array()) for (const auto& item : batch["metas"])
            rememberTitle(imdbId(item),type,name(item));
    }
    for (auto& item : response["metas"]) {
        auto id = text(item,"id"), type = text(item,"type");
        if (imdb(id)) item["name"] = cachedTitle(id,type,text(item,"name"));
    }
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
    if (stream_preferences::get().tmdbToken.empty()) {
        auto translated = publicMeta(id,type);
        auto n = name(translated); if (!n.empty()) meta["name"] = n;
        rememberTitle(id,type,n);
        auto overview = text(translated,"description");
        meta["description"] = overview.empty() ? "Sinopse em português indisponível no momento. Tente novamente mais tarde." : overview;
        if (translated.contains("genres") && translated["genres"].is_array()) meta["genres"] = translated["genres"];
        // Playback IDs, video lists and stream addresses stay with the original addon.
        return;
    }
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
        if (stream_preferences::get().tmdbToken.empty()) {
            auto show = publicMeta(id,"series");
            Json result{{"episodes", Json::array()}};
            if (show.contains("videos") && show["videos"].is_array()) for (const auto& video : show["videos"]) {
                if (!video.is_object() || !video.contains("season") || !video["season"].is_number_integer() || video["season"].get<int>() != number) continue;
                auto ep = video.find("episode");
                if (ep == video.end() || !ep->is_number_integer()) continue;
                result["episodes"].push_back({{"episode_number", *ep}, {"name", text(video,"name")},
                    {"overview", text(video,"overview").empty() ? text(video,"description") : text(video,"overview")}});
            }
            brls::sync([callback,result]() { callback(result); }); return;
        }
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
        if (stream_preferences::get().tmdbToken.empty()) {
            for (const auto& type : {std::string("movie"),std::string("series")}) {
                auto results = request("public-search-" + type + "-" + std::to_string(std::hash<std::string>{}(query)),
                    publicBase + "catalog/" + type + "/tmdb.search/search=" + encoded + ".json", HTTP::Header{"Accept: application/json"});
                if (results.contains("metas") && results["metas"].is_array()) for (auto item : results["metas"]) {
                    if (!item.is_object()) continue;
                    auto id = imdbId(item);
                    if (!imdb(id)) continue;
                    item["id"] = id; item["type"] = type;
                    rememberTitle(id,type,name(item));
                    if (!item.contains("year") || !item["year"].is_string()) item["year"] = text(item,"releaseInfo").substr(0,4);
                    output["metas"].push_back(item);
                }
            }
            brls::sync([callback,output]() { callback(output); }); return;
        }
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
    retryPublicAfter = {}; nextPublicRequest = {};
    std::error_code error;
    for (std::filesystem::directory_iterator i(root, error), end; !error && i != end; i.increment(error))
        if (i->path().extension() == ".json") std::filesystem::remove(i->path(), error);
}
}
