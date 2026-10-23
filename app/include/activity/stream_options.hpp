#pragma once
#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
#include <utility>
#include <tuple>

namespace stream_options {
inline std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
    const std::pair<const char*, const char*> accents[] = {
        {"á", "a"}, {"à", "a"}, {"â", "a"}, {"ã", "a"}, {"ä", "a"},
        {"Á", "a"}, {"À", "a"}, {"Â", "a"}, {"Ã", "a"}, {"Ä", "a"},
        {"é", "e"}, {"è", "e"}, {"ê", "e"}, {"ë", "e"}, {"É", "e"}, {"È", "e"}, {"Ê", "e"}, {"Ë", "e"},
        {"í", "i"}, {"ì", "i"}, {"î", "i"}, {"ï", "i"}, {"Í", "i"}, {"Ì", "i"}, {"Î", "i"}, {"Ï", "i"},
        {"ó", "o"}, {"ò", "o"}, {"ô", "o"}, {"õ", "o"}, {"ö", "o"},
        {"Ó", "o"}, {"Ò", "o"}, {"Ô", "o"}, {"Õ", "o"}, {"Ö", "o"},
        {"ú", "u"}, {"ù", "u"}, {"û", "u"}, {"ü", "u"}, {"Ú", "u"}, {"Ù", "u"}, {"Û", "u"}, {"Ü", "u"},
        {"ç", "c"}, {"Ç", "c"}};
    for (const auto& accent : accents) {
        size_t pos = 0;
        while ((pos = value.find(accent.first, pos)) != std::string::npos) {
            value.replace(pos, std::char_traits<char>::length(accent.first), accent.second); ++pos;
        }
    }
    return value;
}
inline bool preferred(const std::string& raw, const std::string& server) {
    return !server.empty() && lower(raw).find(lower(server)) != std::string::npos;
}
inline bool dubbed(const std::string& raw) {
    return std::regex_search(lower(raw), std::regex("\\b(dublado|dubbed|dual[ -]?audio)\\b"));
}
inline bool subtitled(const std::string& raw) {
    return std::regex_search(lower(raw), std::regex("\\b(legendado|subbed|subtitled)\\b"));
}
inline std::string quality(const std::string& raw) {
    std::smatch match;
    auto text = lower(raw);
    if (std::regex_search(text, match, std::regex("\\b(2160p|1080p|720p|480p|360p|4k)\\b"))) return match.str();
    return "Resolução não informada";
}
inline std::string server(const std::string& raw, const std::string& favorite) {
    if (preferred(raw, favorite)) return favorite;
    for (const char* known : {"Malfoy", "Zunder", "FenixHub"})
        if (preferred(raw, known)) return known;
    auto compact=lower(raw);
    compact.erase(std::remove_if(compact.begin(),compact.end(),[](unsigned char c){return std::isspace(c) || c=='-';}),compact.end());
    if (compact.find("fenixhub")!=std::string::npos || compact.find("phoenixhub")!=std::string::npos) return "FenixHub";
    if (preferred(raw,"Hollymovies")) return "Hollymovies";
    return "Fonte do complemento";
}
inline int sourceRank(const std::string& raw,const std::string& favorite) {
    auto name=server(raw,"");
    if (!favorite.empty() && (preferred(raw,favorite) || lower(name)==lower(favorite))) return -1;
    if (name=="Malfoy") return 0;
    if (name=="FenixHub") return 1;
    return name=="Fonte do complemento" ? 3 : 2;
}
inline int resolution(const std::string& raw) {
    auto q=quality(raw);
    if (q=="2160p" || q=="4k") return 2160;
    if (q=="1080p") return 1080;
    if (q=="720p") return 720;
    if (q=="480p") return 480;
    if (q=="360p") return 360;
    return 0;
}
inline bool before(const std::string& a,const std::string& b,const std::string& favorite) {
    return std::make_tuple(sourceRank(a,favorite),-resolution(a)) < std::make_tuple(sourceRank(b,favorite),-resolution(b));
}
inline std::string audio(const std::string& raw) {
    if (std::regex_search(lower(raw), std::regex("\\bdual[ -]?audio\\b"))) return "Áudio duplo";
    if (dubbed(raw)) return "Dublado";
    if (subtitled(raw)) return "Legendado";
    return "Áudio não informado";
}
}
