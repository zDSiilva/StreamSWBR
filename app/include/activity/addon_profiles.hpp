#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
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
    while (!value.empty() && value.back()=='/') value.pop_back();
    for (const auto& suffix : {std::string("/manifest.json"),std::string("/manifest/json")})
        if (value.size()>suffix.size() && value.compare(value.size()-suffix.size(),suffix.size(),suffix)==0)
            value.resize(value.size()-suffix.size());
    while (!value.empty() && value.back()=='/') value.pop_back();
    const size_t start=value.rfind("https://",0)==0 ? 8 : value.rfind("http://",0)==0 ? 7 : 0;
    if (!start || value.find_first_of(" \t\r\n")!=std::string::npos) return "";
    auto end=value.find_first_of("/?#",start);
    if (value.substr(start,end==std::string::npos ? end : end-start).empty()) return "";
    return value;
}
inline std::string host(const std::string& url) {
    auto start=url.find("://"); if (start==std::string::npos) return "Complemento";
    start+=3; auto end=url.find_first_of("/?#",start);
    auto value=url.substr(start,end==std::string::npos ? end : end-start);
    auto at=value.rfind('@'); if (at!=std::string::npos) value=value.substr(at+1);
    return value;
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
        for (const auto& e:entries) if (e.url==url) return e.enabled; return false;
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
            entries.push_back({trim(name).empty() ? host(url) : trim(name),url});
        } else if (!trim(name).empty()) it->name=trim(name);
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
                    auto flag=item.find("enabled"); entry.enabled=flag!=item.end() && flag->is_boolean() && flag->get<bool>(); break;
                }
            }
        }
        result.reconcile();
        result.imported=normalize(read(data,"importedUrl"));
        return result;
    }
};
}
