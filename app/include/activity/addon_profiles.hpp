#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace addon_profiles {
inline std::string trim(std::string value) {
    auto junk=[](char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='"' || c=='\''; };
    while (!value.empty() && junk(value.front())) value.erase(value.begin());
    while (!value.empty() && junk(value.back())) value.pop_back();
    return value;
}
inline std::string normalize(std::string value) {
    value=trim(value);
    const auto schemeEnd=value.find("://");
    if (schemeEnd==std::string::npos || value.find_first_of(" \t\r\n")!=std::string::npos) return "";
    auto scheme=value.substr(0,schemeEnd);
    std::transform(scheme.begin(),scheme.end(),scheme.begin(),[](unsigned char c){return std::tolower(c);});
    if (scheme!="http" && scheme!="https") return "";
    const auto start=schemeEnd+3, end=value.find_first_of("/?#",start);
    auto authority=value.substr(start,end==std::string::npos ? end : end-start);
    if (authority.empty()) return "";
    const auto credentials=authority.rfind('@');
    auto hostBegin=authority.begin()+(credentials==std::string::npos ? 0 : credentials+1);
    std::transform(hostBegin,authority.end(),hostBegin,[](unsigned char c){return std::tolower(c);});
    const auto port=scheme=="https" ? std::string(":443") : std::string(":80");
    if (authority.size()>port.size() && authority.compare(authority.size()-port.size(),port.size(),port)==0)
        authority.resize(authority.size()-port.size());
    auto path=end==std::string::npos ? std::string{} : value.substr(end);
    while (!path.empty() && path.back()=='/') path.pop_back();
    for (const auto& suffix:{std::string("/manifest.json"),std::string("/manifest/json")})
        if (path.size()>=suffix.size() && path.compare(path.size()-suffix.size(),suffix.size(),suffix)==0)
            path.resize(path.size()-suffix.size());
    while (!path.empty() && path.back()=='/') path.pop_back();
    // Only the public, unconfigured endpoint aliases are interchangeable.
    // Preserve custom paths and query strings: those can represent different configurations.
    if ((authority=="fenixflix.fenixhub.online" || authority=="froststream.cloutteam.com") &&
        (path.empty() || path=="/manifest" || path=="/manifest/json" || path=="/manifest.json")) return "https://"+authority;
    return scheme+"://"+authority+path;
}
inline std::string host(const std::string& url) {
    auto start=url.find("://"); if (start==std::string::npos) return "Complemento";
    start+=3; auto end=url.find_first_of("/?#",start);
    auto value=url.substr(start,end==std::string::npos ? end : end-start);
    auto at=value.rfind('@'); if (at!=std::string::npos) value=value.substr(at+1);
    return value;
}
inline std::string displayName(const std::string& url,std::string name) {
    name=trim(name); auto lower=name;
    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return std::tolower(c);});
    if (url=="https://fenixflix.fenixhub.online" && (name.empty() || lower=="fenixflix" ||
        lower=="phoenixflix" || lower=="fenixflix.fenixhub.online")) return "FenixFlix";
    if (url=="https://froststream.cloutteam.com" && (name.empty() || lower=="froststream" ||
        lower=="frost stream" || lower=="froststream.cloutteam.com")) return "FrostStream";
    return name.empty() ? host(url) : name;
}
struct Entry { std::string name,url; bool enabled=false; };
struct State {
    std::vector<Entry> entries;
    std::string active;
    std::string imported;
    bool multiConfigured=false;
    std::vector<Entry> enabledEntries() const {
        std::vector<Entry> result; for (const auto& e:entries) if (e.enabled) result.push_back(e); return result;
    }
    bool enabled(const std::string& url) const {
        for (const auto& e:entries) if (e.url==normalize(url)) return e.enabled; return false;
    }
    void reconcile() {
        if (enabled(active)) return;
        active.clear(); for (const auto& e:entries) if (e.enabled) { active=e.url; break; }
    }
    bool toggle(const std::string& url) {
        for (auto& e:entries) if (e.url==url) { e.enabled=!e.enabled; reconcile(); return true; } return false;
    }
    void initializeDefaults() {
        if (multiConfigured) return;
        add("FenixFlix","https://fenixflix.fenixhub.online",true);
        add("FrostStream","https://froststream.cloutteam.com",true);
        multiConfigured=true; reconcile();
    }
    bool importUrl(const std::string& input) {
        auto url=normalize(input); if (url.empty() || url==imported) return false;
        imported=url;
        bool known=false; for (const auto& entry:entries) if (entry.url==url) known=true;
        if (!known) { const bool first=entries.empty() && active.empty(); add("",url,first); }
        return true;
    }
    bool add(std::string name,const std::string& input,bool activate=true) {
        auto url=normalize(input); if (url.empty()) return false;
        auto it=std::find_if(entries.begin(),entries.end(),[&](const Entry& e) { return e.url==url; });
        if (it==entries.end()) {
            if (entries.size()>=20) return false;
            entries.push_back({displayName(url,name),url});
        } else if (!trim(name).empty()) it->name=displayName(url,name);
        if (activate) { active=url; for (auto& e:entries) if (e.url==url) e.enabled=true; }
        return true;
    }
    bool select(const std::string& input) {
        auto url=normalize(input);
        for (const auto& entry:entries) if (entry.url==url) { active=url; for (auto& e:entries) if (e.url==url) e.enabled=true; return true; }
        return false;
    }
    bool edit(const std::string& old,std::string name,const std::string& input) {
        auto url=normalize(input); name=trim(name); if (url.empty() || name.empty()) return false;
        auto it=std::find_if(entries.begin(),entries.end(),[&](const Entry& e) { return e.url==old; });
        if (it==entries.end()) return false;
        for (const auto& entry:entries) if (entry.url==url && entry.url!=old) return false;
        const bool wasActive=active==old; const bool wasEnabled=it->enabled; *it={name,url,wasEnabled}; if (wasActive) active=url;
        return true;
    }
    bool remove(const std::string& url) {
        auto it=std::find_if(entries.begin(),entries.end(),[&](const Entry& e) { return e.url==url; });
        if (it==entries.end()) return false;
        entries.erase(it); reconcile();
        return true;
    }
    nlohmann::json json() const {
        auto list=nlohmann::json::array();
        for (const auto& entry:entries) list.push_back({{"name",entry.name},{"url",entry.url},{"enabled",entry.enabled}});
        return {{"url",active},{"addons",list},{"importedUrl",imported},{"multiVersion",multiConfigured ? 1 : 0}};
    }
    static State fromJson(const nlohmann::json& data) {
        State result;
        if (!data.is_object()) return result;
        auto read=[](const nlohmann::json& j,const char* key) {
            auto i=j.find(key); return i!=j.end() && i->is_string() ? i->get<std::string>() : "";
        };
        auto list=data.find("addons");
        if (list!=data.end() && list->is_array()) for (const auto& item:*list)
            result.add(read(item,"name"),read(item,"url"),false);
        auto old=normalize(read(data,"url"));
        if (!old.empty()) result.add("",old,true); // Migrate the existing single-addon setting.
        result.multiConfigured=data.value("multiVersion",0)>=1;
        if (result.multiConfigured && list!=data.end() && list->is_array()) {
            for (auto& entry:result.entries) {
                entry.enabled=false;
                for (const auto& item:*list) if (normalize(read(item,"url"))==entry.url) {
                    auto flag=item.find("enabled"); entry.enabled=entry.enabled || (flag!=item.end() && flag->is_boolean() && flag->get<bool>());
                }
            }
        }
        result.reconcile();
        result.imported=normalize(read(data,"importedUrl"));
        return result;
    }
};
}
