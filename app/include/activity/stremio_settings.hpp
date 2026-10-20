#pragma once
#include <borealis.hpp>

class StremioSettings : public brls::Box {
public:
    explicit StremioSettings(std::function<void()> onRowsChanged);
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override;
private:
    bool rowsChanged = false;
    void edit(int field, brls::Label* label);
    void tmdbHelp(brls::Label* label);
};
