#include "api/network_diagnostics.hpp"
#include <fstream>
#include <mutex>
#include <ctime>
namespace network_diagnostics {
namespace { std::mutex mutex; std::string path; }
void configure(const std::string& directory) { std::lock_guard<std::mutex> lock(mutex); path=directory+"/network-diagnostics.log"; }
void record(const std::string& stage,const std::string& host,int code,int detail) {
    std::lock_guard<std::mutex> lock(mutex); if (path.empty()) return;
    try {
        std::ifstream size(path,std::ios::binary|std::ios::ate);
        bool truncate=size && size.tellg()>131072; size.close();
        std::ofstream out(path,truncate ? std::ios::trunc : std::ios::app);
        out<<std::time(nullptr)<<" "<<stage<<" host="<<host<<" code="<<code<<" detail="<<detail<<"\n";
    } catch (...) {}
}
}
