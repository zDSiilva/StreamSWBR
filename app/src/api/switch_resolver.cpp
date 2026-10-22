#include "api/switch_resolver.hpp"
#include "api/resolver_cache.hpp"
#include "api/network_diagnostics.hpp"
#ifdef __SWITCH__
#include <switch.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <cerrno>
#endif
namespace switch_resolver {
namespace { resolver_cache::Cache cache; }
std::string resolve(const std::string& host) {
#ifdef __SWITCH__
    return cache.get(host,[&]() {
        // Use the console resolver directly instead of curl's asynchronous DNS worker.
        addrinfo hints{}; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
        addrinfo* addresses=nullptr;
        int code=getaddrinfo(host.c_str(),nullptr,&hints,&addresses);
        auto result=resolverGetLastResult();
        network_diagnostics::record("native-dns",host,code,int(result));
        std::string output;
        if (code==0) for (auto* item=addresses;item;item=item->ai_next) {
            if (item->ai_family!=AF_INET || !item->ai_addr) continue;
            char ip[INET_ADDRSTRLEN]{};
            auto* addr=reinterpret_cast<sockaddr_in*>(item->ai_addr);
            if (inet_ntop(AF_INET,&addr->sin_addr,ip,sizeof(ip))) {
                if (!output.empty()) output+=",";
                output+=ip;
            }
        }
        if (addresses) freeaddrinfo(addresses);
        return output;
    });
#else
    return "";
#endif
}
void invalidate(const std::string& host) { cache.invalidate(host); }
}
