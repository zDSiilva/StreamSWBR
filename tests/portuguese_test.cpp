#include "activity/stream_options.hpp"
#include "activity/resume_policy.hpp"
#include <limits>
#include "activity/addon_profiles.hpp"
#include "activity/stream_preferences.hpp"
#include "api/portuguese_metadata.hpp"
#include "api/http.hpp"
#include <cassert>
#include <filesystem>
#include <future>
#include <iostream>

int main() {
    assert(resume_policy::seekable(120,1200));
    assert(resume_policy::seekable(120,0));
    assert(!resume_policy::seekable(1195,1200));
    assert(!resume_policy::seekable(-20,1200));
    assert(!resume_policy::seekable(std::numeric_limits<double>::infinity(),1200));
    assert(!resume_policy::recordable(std::numeric_limits<double>::quiet_NaN(),1200));
    assert(!resume_policy::recordable(120,-1));
    assert(resume_policy::recordable(120,1200));
    using addon_profiles::State;
    auto fresh=State::fromJson(nlohmann::json::object());
    assert(fresh.entries.empty() && fresh.active.empty());
    assert(addon_profiles::normalize(" https://example.org/a/manifest/json/ ")=="https://example.org/a");
    assert(addon_profiles::normalize("https:///manifest.json").empty());
    assert(addon_profiles::normalize("ftp://example.org").empty());
    auto migrated=State::fromJson({{"url","https://example.org/old/manifest.json"}});
    assert(migrated.entries.size()==1 && migrated.active=="https://example.org/old");
    assert(migrated.add("Segundo","https://example.net/config/manifest.json"));
    assert(migrated.entries.size()==2 && migrated.active=="https://example.net/config");
    assert(migrated.select("https://example.org/old/manifest.json"));
    assert(migrated.entries.size()==2);
    assert(migrated.add("Renomeado","https://example.org/old/"));
    assert(migrated.entries.size()==2 && migrated.entries[0].name=="Renomeado");
    assert(!migrated.edit(migrated.active,"Duplicado","https://example.net/config"));
    assert(migrated.edit(migrated.active,"Novo nome","https://example.org/new"));
    assert(migrated.active=="https://example.org/new");
    assert(migrated.importUrl("https://example.net/config/manifest.json"));
    assert(migrated.active=="https://example.org/new"); // Import cannot undo selection.
    auto restored=State::fromJson(migrated.json());
    assert(restored.entries.size()==2 && restored.active==migrated.active);
    assert(restored.remove("https://example.net/config"));
    assert(!restored.importUrl("https://example.net/config")); // Deleted import stays deleted.
    assert(restored.remove(restored.active));
    assert(restored.active.empty() && restored.entries.empty());
    assert(State::fromJson(restored.json()).active.empty());
    State firstImport; assert(firstImport.importUrl("https://example.org/first"));
    assert(firstImport.active=="https://example.org/first");
    auto malformed=State::fromJson({{"url",nullptr},{"addons",nlohmann::json::array({42,{{"name",true},{"url","https://example.org"}}})}});
    assert(malformed.entries.size()==1 && malformed.active.empty());
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
    nlohmann::json batch = {{"metas", nlohmann::json::array({{{"id","tt1375666"},{"type","movie"},{"name","Inception"}},
        {{"id","tt999"},{"type","movie"},{"name","Original"}}})}};
    calls = HTTP::calls;
    portuguese_metadata::localizeCatalog(batch);
    assert(HTTP::calls == calls + 2);
    assert(batch["metas"][0]["name"] == "A Origem" && batch["metas"][0]["id"] == "tt1375666");
    assert(batch["metas"][1]["name"] == "Original");
    calls = HTTP::calls;
    std::string immediate;
    portuguese_metadata::title("tt1375666","movie",[&](std::string name) { immediate = name; });
    assert(immediate == "A Origem" && HTTP::calls == calls);
    assert(std::filesystem::exists(directory / "metadata-pt/title-movie-tt1375666.json"));
    portuguese_metadata::localizeCatalog(batch); assert(HTTP::calls == calls);
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
