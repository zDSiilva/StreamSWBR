#pragma once
#include <string>
#include <cmath>
namespace playback_policy {
inline int score(const std::string& provider,const std::string& host,const std::string& quality,const std::string& audio,
    const std::string& candidateProvider,const std::string& candidateHost,const std::string& candidateQuality,const std::string& candidateAudio) {
    if (provider.empty() && host.empty()) return -1;
    if (!provider.empty() && provider!="Fonte do complemento") {
        if (candidateProvider!=provider) return -1;
    } else if (host.empty() || host!=candidateHost) return -1;
    return 1 + (host==candidateHost ? 1 : 0) + (quality==candidateQuality ? 4 : 0) + (audio==candidateAudio ? 8 : 0);
}
inline bool countdown(double position,double duration) { return std::isfinite(position) && std::isfinite(duration) && duration>30 && position>=duration-30 && position<=duration; }
}
