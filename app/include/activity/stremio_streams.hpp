/*
    Stremio stream picker

    A scrollable, selectable list of streams for a chosen movie/episode. Built
    on the same RecyclingGrid that the rest of the app uses (so it works with
    both touch and the D-pad). Selecting a row plays that stream.
*/
#pragma once

#include <borealis.hpp>
#include "api/stremio.hpp"
#include "activity/stremio_resume.hpp"

class RecyclingGrid;

class StreamPicker : public brls::Box {
public:
    StreamPicker(const std::string& title, const std::vector<stremio::Stream>& streams, const ResumeEntry& resumeKey);

    ~StreamPicker() override;

private:
    void searchAlternate();
    std::string origin;
    brls::Label* addonLabel=nullptr;
    brls::Label* alternateLabel=nullptr;
    brls::Box* alternateButton=nullptr;
    stream_worker::Cancel lookup;
    std::shared_ptr<std::atomic_bool> alive=std::make_shared<std::atomic_bool>(true);
    bool searching=false;
    void refresh();
    std::vector<stremio::Stream> streams;
    ResumeEntry resumeKey;
    brls::Label* filterLabel = nullptr;
    brls::Box* filterButton = nullptr;
    int filter = 0;
    RecyclingGrid* recycler = nullptr;
};
