#pragma once
#include <functional>
namespace brls { inline void sync(const std::function<void()>& callback) { callback(); } }
