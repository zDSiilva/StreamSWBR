/*
    Stremio favourites

    A small persistent list of favourited movies/series (saved to
    configDir/favourites.json), plus FavCardCell — a poster card whose X button
    toggles favourite status.
*/
#pragma once

#include <borealis.hpp>
#include "view/video_card.hpp"
#include "api/stremio.hpp"
#include "api/portuguese_metadata.hpp"

// Poster card with X = toggle favourite. The closure is (re)set in cellForRow.
class FavCardCell : public VideoCardCell {
public:
    void staticTitle(const std::string& name) {
        ++titleTicket;
        this->labelTitle->setText(name);
    }
    void bindTitle(const std::string& id, const std::string& type, const std::string& original, const std::string& suffix = "") {
        const auto ticket = ++titleTicket;
        this->labelTitle->setText(portuguese_metadata::cachedTitle(id, type, original) + suffix);
        ASYNC_RETAIN
        portuguese_metadata::title(id, type, [ASYNC_TOKEN, ticket, suffix](const std::string& name) {
            ASYNC_RELEASE
            if (ticket == titleTicket && !name.empty()) this->labelTitle->setText(name + suffix);
        });
    }
    FavCardCell() {
        // Two constrained lines preserve more of the title without marquee fragments.
        this->labelTitle->setFontSize(17);
        this->labelTitle->setSingleLine(false);
        this->labelTitle->setWidthPercentage(100);
        this->labelTitle->setHeight(44);
        this->labelTitle->setShrink(0);
        this->labelTitle->setLineHeight(1.15f);
        this->labelTitle->setMarginTop(10);
        this->labelTitle->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        this->labelTitle->getParent()->setShrink(0);
        this->labelTitle->setAutoAnimate(false);
        this->registerAction("Biblioteca", brls::BUTTON_X, [this](brls::View*) {
            if (this->onToggleFav) this->onToggleFav();
            return true;
        });
    }

    std::function<void()> onToggleFav;

    static RecyclingGridItem* create() { return new FavCardCell(); }
private:
    size_t titleTicket = 0;
};

// Persistent favourites (movies + series). Survives restarts.
class Favourites {
public:
    static Favourites& instance();

    void load();                                  // read from disk once
    bool contains(const std::string& id) const;   // is this id favourited?
    bool toggle(const stremio::Meta& m);          // add/remove; returns true if now favourited
    const std::vector<stremio::Meta>& all() const { return items; }
    brls::VoidEvent* changed() { return &changedEvent; }  // fires on any add/remove

private:
    void save();
    bool loaded = false;
    std::vector<stremio::Meta> items;
    brls::VoidEvent changedEvent;
};
