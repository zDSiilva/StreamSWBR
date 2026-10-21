#pragma once
#include <nlohmann/json.hpp>
#include <string>
struct ResumeEntry {
    std::string streamType, streamId, name, poster;
    double position=0,duration=0;
    long updated=0;
    bool enriched=false;
    std::string sourceProvider,sourceHost,sourceQuality,sourceAudio,sourceAddon;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ResumeEntry,streamType,streamId,name,poster,position,duration,updated,enriched,
    sourceProvider,sourceHost,sourceQuality,sourceAudio,sourceAddon);
