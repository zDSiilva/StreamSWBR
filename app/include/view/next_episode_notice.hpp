#pragma once
#include <borealis.hpp>
// Fixed two-line drawing: independent of Label wrapping/Yoga measurement.
class NextEpisodeNotice : public brls::Label {
public:
    void draw(NVGcontext* vg,float x,float y,float w,float h,brls::Style,brls::FrameContext*) override {
        auto text=getFullText(); if (text.empty() || w<60 || h<40) return;
        auto separator=text.find('\n');
        auto heading=text.substr(0,separator);
        auto detail=separator==std::string::npos ? std::string{} : text.substr(separator+1);
        nvgSave(vg);
        nvgIntersectScissor(vg,x,y,w,h);
        nvgBeginPath(vg); nvgRoundedRect(vg,x+1,y+1,w-2,h-2,12);
        nvgFillColor(vg,nvgRGBA(12,22,34,242)); nvgFill(vg);
        nvgStrokeColor(vg,nvgRGBA(106,219,168,85)); nvgStrokeWidth(vg,1); nvgStroke(vg);
        nvgBeginPath(vg); nvgMoveTo(vg,x+22,y+30); nvgLineTo(vg,x+22,y+49);
        nvgLineTo(vg,x+36,y+39.5f); nvgClosePath(vg);
        nvgFillColor(vg,nvgRGB(116,231,179)); nvgFill(vg);
        nvgIntersectScissor(vg,x+52,y+10,w-66,h-20);
        nvgFontFaceId(vg,getFont()); nvgFontQuality(vg,getFontQuality());
        nvgTextAlign(vg,NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
        nvgFontSize(vg,18); nvgFillColor(vg,nvgRGB(238,245,242));
        nvgText(vg,x+52,y+17,heading.c_str(),nullptr);
        nvgFontSize(vg,15); nvgFillColor(vg,nvgRGB(166,190,182));
        nvgText(vg,x+52,y+45,detail.c_str(),nullptr);
        nvgRestore(vg);
    }
};
