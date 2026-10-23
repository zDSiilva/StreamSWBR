#pragma once
#include <borealis.hpp>
#include "api/stremio.hpp"
#include "activity/stremio_resume.hpp"
class RecyclingGrid;
class StreamPicker : public brls::Box {
public:
    struct Group {
        addon_profiles::Entry addon;
        std::vector<stremio::Stream> streams;
        bool pending=true, expanded=false;
        std::string error;
    };
    StreamPicker(const std::string& title,const std::vector<stremio::Stream>& streams,const ResumeEntry& resumeKey);
    ~StreamPicker() override;
private:
    void fetchGroup(size_t index);
    void refresh();
    void toggle(size_t index);
    std::vector<Group> groups;
    std::vector<std::string> rowKeys;
    std::vector<stream_worker::Cancel> lookups;
    std::shared_ptr<std::atomic_bool> alive=std::make_shared<std::atomic_bool>(true);
    ResumeEntry resumeKey;
    brls::Label* filterLabel=nullptr;
    brls::Box* filterButton=nullptr;
    int filter=0;
    RecyclingGrid* recycler=nullptr;
};
