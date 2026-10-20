#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
class HTTP {
public:
    using Header = std::vector<std::string>;
    struct Timeout { long timeout; };
    inline static int calls = 0;
    inline static bool authorized = true;
    static std::string get(const std::string& url, const Header& headers, Timeout) {
        ++calls;
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
