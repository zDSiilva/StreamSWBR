#include "activity/stream_options.hpp"
#include "activity/stream_preferences.hpp"
#include "api/portuguese_metadata.hpp"
#include "api/http.hpp"
#include <cassert>
#include <filesystem>
#include <future>
#include <iostream>

int main() {
    using namespace stream_options;
    assert(preferred("FenixFlix 1080p · MALFOY", "Malfoy"));
    assert(!preferred("FenixHub", ""));
    assert(!preferred("Zunder", "Malfoy"));
    assert(lower("AÇÃO E COMÉDIA") == "acao e comedia");
    assert(quality("FenixFlix 1080p Dublado Malfoy") == "1080p");
    assert(quality("Direct MP4") == "Resolução não informada");
    assert(dubbed("BR Dublado (Opção 1)"));
    assert(dubbed("Dual Audio"));
    assert(!dubbed("Legendado"));
    assert(subtitled("US Legendado Malfoy"));
    assert(!subtitled("BR Dublado Malfoy"));
    assert(audio("Unknown source") == "Áudio não informado");
    const auto directory = std::filesystem::temp_directory_path() /
        ("streamfin-pt-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    stream_preferences::load(directory.string());
    auto prefs = stream_preferences::get(); prefs.tmdbToken = "fixture-token"; prefs.preferredServer = "Malfoy";
    assert(stream_preferences::save(prefs));
    stream_preferences::load(directory.string());
    assert(stream_preferences::get().preferredServer == "Malfoy");
    auto withoutToken = stream_preferences::get(); withoutToken.tmdbToken.clear();
    assert(stream_preferences::save(withoutToken));
    { std::ofstream import(directory / "addon.txt"); import << "# ignored\ntmdb_token=fixture-token\r\n"; }
    stream_preferences::importFile(directory.string());
    assert(stream_preferences::get().tmdbToken == "fixture-token");
    portuguese_metadata::configure(directory.string());
    portuguese_metadata::clearCache();
    nlohmann::json movie = {{"meta", {{"id", "tt1375666"}, {"type", "movie"}, {"name", "Inception"},
        {"description", "English overview"}, {"videos", nlohmann::json::array()}}}};
    portuguese_metadata::localizeDetail(movie);
    assert(movie["meta"]["id"] == "tt1375666");
    assert(movie["meta"]["name"] == "A Origem");
    assert(movie["meta"]["description"] == "Uma sinopse em português.");
    assert(movie["meta"]["genres"][0] == "Ficção científica");
    int calls = HTTP::calls;
    portuguese_metadata::localizeDetail(movie);
    assert(HTTP::calls == calls); // Cache must prevent duplicate network requests.
    assert(portuguese_metadata::cachedTitle("tt1375666", "movie", "Inception") == "A Origem");
    std::promise<nlohmann::json> episodes;
    auto ready = episodes.get_future();
    portuguese_metadata::season("tt0944947", 1, [&episodes](nlohmann::json j) { episodes.set_value(j); });
    assert(ready.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    assert(ready.get()["episodes"][0]["name"] == "O inverno está chegando");
    std::promise<nlohmann::json> search;
    auto searchReady = search.get_future();
    portuguese_metadata::search("A Origem", [&search](nlohmann::json j) { search.set_value(j); });
    assert(searchReady.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    auto searchResults = searchReady.get();
    assert(searchResults["metas"][0]["id"] == "tt1375666");
    assert(searchResults["metas"][0]["name"] == "A Origem");
    nlohmann::json unknown = {{"meta", {{"id", "tt99999999"}, {"type", "movie"}, {"name", "Original"}}}};
    portuguese_metadata::localizeDetail(unknown);
    assert(unknown["meta"]["name"] == "Original");
    assert(unknown["meta"]["description"] == "Sinopse em português indisponível.");
    portuguese_metadata::clearCache();
    HTTP::authorized = false;
    portuguese_metadata::localizeDetail(movie);
    assert(movie["meta"]["id"] == "tt1375666");
    assert(movie["meta"]["description"] == "Sinopse em português indisponível.");
    portuguese_metadata::clearCache();
    HTTP::authorized = true;
    auto automatic = stream_preferences::get(); automatic.tmdbToken.clear(); stream_preferences::save(automatic);
    movie["meta"]["videos"] = nlohmann::json::array({{{"id","tt1375666"}}});
    portuguese_metadata::localizeDetail(movie);
    assert(movie["meta"]["name"] == "A Origem");
    assert(movie["meta"]["description"] == "Sinopse pública em português.");
    assert(movie["meta"]["id"] == "tt1375666");
    assert(movie["meta"]["videos"][0]["id"] == "tt1375666");
    assert(portuguese_metadata::cachedTitle("tt1375666","movie","Inception") == "A Origem");
    calls = HTTP::calls; portuguese_metadata::localizeDetail(movie); assert(HTTP::calls == calls);
    std::promise<nlohmann::json> publicEpisodes; auto pe = publicEpisodes.get_future();
    portuguese_metadata::season("tt0898266",1,[&](nlohmann::json j) { publicEpisodes.set_value(j); });
    assert(pe.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    auto eps = pe.get()["episodes"]; assert(eps.size() == 1 && eps[0]["name"] == "Piloto");
    std::promise<nlohmann::json> publicSearch; auto ps = publicSearch.get_future();
    portuguese_metadata::search("A Origem",[&](nlohmann::json j) { publicSearch.set_value(j); });
    assert(ps.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    auto pm = ps.get()["metas"]; assert(pm.size() == 1 && pm[0]["id"] == "tt1375666");
    assert(pm[0]["year"].is_string());
    portuguese_metadata::clearCache(); HTTP::authorized = false;
    portuguese_metadata::localizeDetail(movie);
    assert(movie["meta"]["id"] == "tt1375666");
    assert(movie["meta"]["description"] == "Sinopse em português indisponível no momento. Tente novamente mais tarde.");
    portuguese_metadata::clearCache();
    std::filesystem::remove_all(directory);
    std::cout << "OK: classification, settings, Portuguese metadata, episodes, cache and failure fallback\n";
}
