#pragma once
#include <borealis.hpp>
#include "activity/addon_profiles.hpp"

class StremioAddons : public brls::Box {
public:
    explicit StremioAddons(std::function<void()> onClose = {});
    void draw(NVGcontext*,float,float,float,float,brls::Style,brls::FrameContext*) override;
private:
    brls::Box* list = nullptr;
    brls::Button* addButton = nullptr;
    void rebuild();
    void refresh();
    void actions(addon_profiles::Entry entry);
    void askUrl(addon_profiles::Entry entry = {});
    void askName(addon_profiles::Entry entry,const std::string& url);
    void commit(const addon_profiles::State& next);
};
