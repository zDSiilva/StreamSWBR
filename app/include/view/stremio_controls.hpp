#pragma once
#include "view/stremio_theme.hpp"
#include <initializer_list>
#include <utility>
namespace stremio_theme {
inline brls::Box* controls(brls::Box* parent,
    std::initializer_list<std::pair<const char*, const char*>> items, bool compact=false) {
    auto* row = new brls::Box(); row->setAxis(brls::Axis::ROW);
    row->setHeight(compact ? 24 : 44); row->setShrink(0); row->setMarginTop(compact ? 2 : 12);
    for (const auto& item : items) {
        auto* pill = new brls::Box(); pill->setAxis(brls::Axis::ROW);
        pill->setPadding(compact ? 2 : 8,compact ? 6 : 12,compact ? 2 : 8,compact ? 6 : 12); pill->setCornerRadius(8); pill->setMarginRight(12);
        const char key = item.first[0];
        pill->setBackgroundColor(compact ? nvgRGBA(0,0,0,0) : key == 'A' ? nvgRGB(25,101,74) :
            key == 'B' ? nvgRGB(132,46,62) : key == 'Y' ? nvgRGB(120,88,26) : nvgRGB(42,70,130));
        auto* label = new brls::Label();
        label->setText(std::string(item.first)+"  "+item.second);
        label->setFontSize(compact ? 12 : 16); label->setTextColor(compact ? TEXT_DIM : TEXT); label->setSingleLine(true);
        pill->addView(label); row->addView(pill);
    }
    parent->addView(row); return row;
}
class RadioDot : public brls::View {
public:
    explicit RadioDot(bool selected) : selected(selected) { setDimensions(24,32); setShrink(0); setMarginRight(10); }
    void draw(NVGcontext* vg,float x,float y,float w,float h,brls::Style,brls::FrameContext*) override {
        nvgBeginPath(vg); nvgCircle(vg,x+w/2,y+h/2,9); nvgStrokeColor(vg,TEXT_DIM); nvgStrokeWidth(vg,2); nvgStroke(vg);
        if (selected) { nvgBeginPath(vg); nvgCircle(vg,x+w/2,y+h/2,5); nvgFillColor(vg,nvgRGB(120,235,200)); nvgFill(vg); }
    }
private: bool selected;
};
}
