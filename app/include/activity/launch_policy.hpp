#pragma once
#include <algorithm>
#include <cctype>
#include <string>
namespace launch_policy {
inline bool mediaArgument(std::string arg) {
    std::transform(arg.begin(),arg.end(),arg.begin(),[](unsigned char c){return std::tolower(c);});
    auto path=arg.substr(0,arg.find_first_of("?#"));
    for (const char* extension:{".nro",".nsp",".nacp"}) {
        std::string ext=extension;
        if (path.size()>=ext.size() && path.compare(path.size()-ext.size(),ext.size(),ext)==0) return false;
    }
    if (arg.rfind("https://",0)==0 || arg.rfind("http://",0)==0) return true;
    for (const char* extension:{".mp4",".mkv",".webm",".avi",".mov",".m3u8",".m3u",".ts"}) {
        std::string ext=extension;
        if (path.size()>=ext.size() && path.compare(path.size()-ext.size(),ext.size(),ext)==0) return true;
    }
    return false;
}
}
