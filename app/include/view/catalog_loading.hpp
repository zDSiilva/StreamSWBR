#pragma once
#include <borealis.hpp>
#include "api/loading_activity.hpp"
#include <chrono>
#include <cmath>
// Draw-only status: never takes focus or prevents browsing.
class CatalogLoading : public brls::Label {
    using Clock=std::chrono::steady_clock;
    Clock::time_point began{}, shown{};
    bool waiting=false, visible=false;
public:
    void draw(NVGcontext* vg,float x,float y,float w,float h,brls::Style,brls::FrameContext*) override {
        auto now=Clock::now(); bool busy=loading_activity::busy();
        if (busy && !waiting) { began=now; waiting=true; }
        if (!busy) waiting=false;
        if (busy && !visible && now-began>=std::chrono::milliseconds(250)) { visible=true; shown=now; }
        if (!busy && visible && now-shown>=std::chrono::milliseconds(450)) visible=false;
        if (!visible || w<80) return;
        // Modulo in double precision keeps animation smooth after long uptimes.
        float t=float(std::fmod(std::chrono::duration<double>(now.time_since_epoch()).count(),2.0));
        nvgSave(vg); nvgIntersectScissor(vg,x,y,w,h);
        nvgBeginPath(vg); nvgArc(vg,x+7,y+9,4,t*3.14159f,t*3.14159f+4.2f,NVG_CW);
        nvgStrokeWidth(vg,1.4f); nvgStrokeColor(vg,nvgRGBA(115,219,174,205)); nvgStroke(vg);
        nvgFontFaceId(vg,getFont()); nvgFontQuality(vg,getFontQuality()); nvgFontSize(vg,12);
        nvgTextAlign(vg,NVG_ALIGN_LEFT|NVG_ALIGN_TOP); nvgFillColor(vg,nvgRGBA(172,198,187,215));
        nvgText(vg,x+19,y+2,"Carregando informações…",nullptr);
        nvgBeginPath(vg); nvgRoundedRect(vg,x,y+h-2,w,2,1);
        nvgFillColor(vg,nvgRGBA(85,160,129,35)); nvgFill(vg);
        float segment=w*.23f, offset=(t/2.0f)*(w+segment)-segment;
        nvgBeginPath(vg); nvgRoundedRect(vg,x+offset,y+h-2,segment,2,1);
        nvgFillColor(vg,nvgRGBA(97,218,158,205)); nvgFill(vg); nvgRestore(vg);
    }
};
