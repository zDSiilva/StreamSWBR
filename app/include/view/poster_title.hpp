#pragma once
#include <borealis.hpp>
#include <algorithm>
#include <vector>

// Fixed-size poster text: wrapping is independent of Yoga's measure pass.
// A scissor also guarantees that no glyph can spill into adjacent cards.
class PosterTitle : public brls::Label {
public:
    static brls::View* create() { return new PosterTitle(); }
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style, brls::FrameContext*) override {
        const auto text = getFullText();
        const float usable = std::max(0.0f, width - 12);
        if (usable <= 0 || height <= 0 || text.empty()) return;
        nvgSave(vg);
        nvgIntersectScissor(vg, x, y, width, height);
        nvgFontFaceId(vg, getFont()); nvgFontSize(vg, getFontSize());
        nvgFontQuality(vg, getFontQuality());
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        const float step = getFontSize() * getLineHeight();
        const int limit = std::max(1, std::min(2, int(height / step)));
        if (text != measuredText || usable != measuredWidth || getFontSize() != measuredSize || limit != measuredLines) {
            measuredText = text; measuredWidth = usable; measuredSize = getFontSize(); measuredLines = limit;
            lines.clear();
            NVGtextRow rows[3];
            const int count = nvgTextBreakLines(vg, text.c_str(), nullptr, usable, rows, limit + 1);
            for (int i = 0; i < std::min(count, limit); ++i) lines.emplace_back(rows[i].start, rows[i].end);
            if (count > limit && !lines.empty()) {
                auto& last = lines.back();
                while (!last.empty() && nvgTextBounds(vg, 0, 0, (last + "…").c_str(), nullptr, nullptr) > usable) {
                    size_t pos = last.size() - 1;
                    while (pos > 0 && (static_cast<unsigned char>(last[pos]) & 0xc0) == 0x80) --pos;
                    last.resize(pos);
                }
                last += "…";
            }
        }
        nvgFillColor(vg, a(getTextColor()));
        const float top = y + std::max(0.0f, (height - step * lines.size()) / 2);
        for (size_t i = 0; i < lines.size(); ++i) nvgText(vg, x + width / 2, top + step * i, lines[i].c_str(), nullptr);
        nvgRestore(vg);
    }
private:
    std::string measuredText;
    float measuredWidth = -1, measuredSize = -1;
    int measuredLines = 0;
    std::vector<std::string> lines;
};
