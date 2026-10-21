#pragma once
#include "activity/stremio_resume.hpp"
namespace stremio_playback {
void play(ResumeEntry key,const stremio::Stream& stream);
void resume(const ResumeEntry& key);
void interruptResume();
void attach(std::function<void(const std::string&,const std::string&)> replace);
void cancel();
void prepareSubtitles(const std::string& type,const std::string& id);
bool ended();
void failed();
std::string notice(double position,double duration);
}
