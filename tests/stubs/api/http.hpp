#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <functional>
class HTTP {
public:
    using Header = std::vector<std::string>;
    struct FreshDNS {};
    using Cancel=std::shared_ptr<std::atomic_bool>;
    struct Timeout { long timeout; };
    inline static int calls = 0;
    inline static bool authorized = true;
    inline static std::function<std::string(const std::string&,Cancel)> transport;
    static std::string get(const std::string& url,Timeout,Cancel cancel,FreshDNS) { return transport(url,cancel); }
    static std::string get(const std::string& url,Timeout timeout) { return get(url,Header{},timeout); }
    static std::string get(const std::string& url, const Header& headers, Timeout) {
        ++calls;
        if (url.rfind("https://tmdb.elfhosted.com/",0) == 0) {
            if (headers != Header{"Accept: application/json"}) throw std::runtime_error("public service must not receive credentials");
            if (!authorized) throw std::runtime_error("public service unavailable");
            if (url.find("/catalog/movie/tmdb.top.json") != std::string::npos)
                return R"({"metas":[{"id":"tmdb:27205","name":"A Origem","links":[{"category":"imdb","url":"https://imdb.com/title/tt1375666"}]},{"id":"tmdb:999","name":"ID inválido","links":[{"category":"imdb","url":"https://imdb.com.evil.test/title/tt999"}]}]})";
            if (url.find("/meta/movie/tt1375666.json") != std::string::npos)
                return R"({"meta":{"id":"tmdb:27205","name":"A Origem","description":"Sinopse pública em português.","genres":["Ficção científica"],"videos":[{"id":"must-not-replace-playback"}]}})";
            if (url.find("/meta/series/tt0898266.json") != std::string::npos)
                return R"({"meta":{"name":"Big Bang: A Teoria","videos":[{"season":1,"episode":1,"name":"Piloto","overview":"Sinopse do piloto."},{"season":2,"episode":1,"name":"Outra temporada"}]}})";
            if (url.find("/catalog/movie/tmdb.search/") != std::string::npos)
                return R"({"metas":[{"id":"tmdb:27205","imdb_id":"tt1375666","name":"A Origem","year":2010},{"id":"tmdb:999","name":"Sem IMDb"}]})";
            return R"({"metas":[]})";
        }
        if (headers.empty() || headers[0] != "Authorization: Bearer fixture-token") throw std::runtime_error("bad auth");
        if (!authorized) return R"({"success":false})";
        if (url.find("/search/multi?") != std::string::npos)
            return R"({"results":[{"id":27205,"media_type":"movie","title":"A Origem","release_date":"2010-07-16","poster_path":"/poster.jpg"}]})";
        if (url.find("/movie/27205/external_ids?") != std::string::npos)
            return R"({"imdb_id":"tt1375666"})";
        if (url.find("/find/tt1375666?") != std::string::npos)
            return R"({"movie_results":[{"id":27205,"title":"A Origem","overview":"Uma sinopse em português."}]})";
        if (url.find("/movie/27205?") != std::string::npos)
            return R"({"id":27205,"title":"A Origem","overview":"Uma sinopse em português.","genres":[{"name":"Ficção científica"}]})";
        if (url.find("/find/tt0944947?") != std::string::npos)
            return R"({"tv_results":[{"id":1399,"name":"Game of Thrones"}]})";
        if (url.find("/tv/1399/season/1?") != std::string::npos)
            return R"({"episodes":[{"episode_number":1,"name":"O inverno está chegando","overview":"Sinopse do episódio."}]})";
        return R"({"movie_results":[],"tv_results":[]})";
    }
};
