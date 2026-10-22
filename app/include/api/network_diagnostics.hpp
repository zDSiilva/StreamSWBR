#pragma once
#include <string>
namespace network_diagnostics {
void configure(const std::string& directory);
void record(const std::string& stage,const std::string& host,int code,int detail=0);
}
