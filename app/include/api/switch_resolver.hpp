#pragma once
#include <string>
namespace switch_resolver {
std::string resolve(const std::string& host);
void invalidate(const std::string& host);
}
