#pragma once
#include <functional>
#include <vector>
namespace brls {
inline std::vector<std::function<void()>> metadataTasks;
inline void sync(const std::function<void()>& callback) { callback(); }
inline void async(const std::function<void()>& callback) { metadataTasks.push_back(callback); }
}
