#pragma once
#include "activity/stremio_home.hpp"
#include "activity/stremio_playback.hpp"
#include "view/mpv_core.hpp"
namespace home_navigation {
inline bool moving=false;
inline void step() {
    auto stack=brls::Application::getActivitiesStack();
    if (stack.empty() || dynamic_cast<StremioHome*>(stack.back()->getContentView()) || stack.size()<=1) { moving=false; return; }
    if (!brls::Application::popActivity(brls::TransitionAnimation::NONE,[]() { brls::sync([](){step();}); })) moving=false;
}
inline void install() {
    brls::Application::getRunLoopEvent()->subscribe([]() {
        static bool wasDown=false;
        bool down=brls::Application::getControllerState().buttons[brls::BUTTON_RT];
        bool pressed=down && !wasDown; wasDown=down;
        if (!pressed || moving || brls::Application::isInputBlocks()) return;
        auto stack=brls::Application::getActivitiesStack();
        bool hasHome=false; for (auto* activity:stack) if (dynamic_cast<StremioHome*>(activity->getContentView())) hasHome=true;
        if (!hasHome || stack.empty() || dynamic_cast<StremioHome*>(stack.back()->getContentView())) return;
        ResumeTracker::instance().checkpoint();
        if (stream_worker::activeForeground) stream_worker::activeForeground->cancelPending();
        if (stream_worker::activePrefetch) stream_worker::activePrefetch->cancelPending();
        stream_worker::cancelSearch();
        stremio_playback::cancel();
        moving=true;
        brls::sync([](){step();});
    });
}
}
