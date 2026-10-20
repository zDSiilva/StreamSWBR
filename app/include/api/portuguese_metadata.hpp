#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <string>

namespace portuguese_metadata {
// All network work is off the UI thread. IDs and playback URLs are untouched.
void configure(const std::string& directory);
std::string cachedTitle(const std::string& id, const std::string& type, const std::string& fallback);
void title(const std::string& id, const std::string& type, std::function<void(std::string)> callback);
void localizeDetail(nlohmann::json& response);
void localizeCatalog(nlohmann::json& response);
void season(const std::string& id, int number, std::function<void(nlohmann::json)> callback);
void search(const std::string& query, std::function<void(nlohmann::json)> callback);
void clearCache();
}
