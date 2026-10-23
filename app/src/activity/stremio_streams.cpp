#include "view/stremio_controls.hpp"
/*
    Stremio stream picker -- implementation.

    A compact, scrollable, selectable list of streams. Uses the file-list row
    layout (small static icon + text, no thumbnails to load) so it scrolls fast.
*/
#include "activity/stremio_streams.hpp"
#include "activity/stremio_resume.hpp"
#include "view/recycling_grid.hpp"
#include "view/svg_image.hpp"
#include "view/mpv_core.hpp"
#include "tab/remote_view.hpp"
#include "api/stremio.hpp"
#include "activity/stream_preferences.hpp"
#include "activity/stream_options.hpp"
#include "activity/stremio_playback.hpp"
#include "view/stremio_theme.hpp"

#include <algorithm>

using namespace brls::literals;

namespace {

// Fetches subtitles from the configured subtitles addon (SubSource etc.) for
// the title being played and attaches them to MPV once the file is loaded.
// They then appear in the player's + menu Subtitle picker alongside any
// embedded tracks.
class SubLoader {
public:
    static SubLoader& instance() {
        static SubLoader inst;
        return inst;
    }

    // Call right before starting playback of {type, id}.
    void prepare(const std::string& type, const std::string& id) {
        this->init();
        this->pending.clear();
        this->fileLoaded = false;
        int ticket = ++this->requestTicket;
        if (stremio::SUBTITLES_ADDON.empty() || stream_preferences::get().subtitles == "off") return;
        std::string url = stremio::SUBTITLES_ADDON + "/subtitles/" + type + "/" + id + ".json";
        stremio::getJSON<stremio::SubtitleList>(
            [this, ticket](stremio::SubtitleList r) {
                if (ticket != this->requestTicket) return;  // a newer title started
                // Keep the first subtitle per language, capped, so the picker
                // stays usable (addons can return dozens per release).
                std::vector<std::string> seen;
                for (auto& s : r.subtitles) {
                    if (this->pending.size() >= 12) break;
                    if (std::find(seen.begin(), seen.end(), s.lang) != seen.end()) continue;
                    seen.push_back(s.lang);
                    this->pending.push_back(s);
                }
                if (this->fileLoaded) this->attach();
            },
            [](const std::string& e) { brls::Logger::warning("subtitles addon: {}", e); }, url);
    }

private:
    void init() {
        if (this->inited) return;
        this->inited = true;
        MPVCore::instance().getEvent()->subscribe([this](MpvEventEnum e) {
            switch (e) {
            case MpvEventEnum::MPV_LOADED:
                this->fileLoaded = true;
                this->attach();
                break;
            case MpvEventEnum::MPV_STOP:
            case MpvEventEnum::END_OF_FILE:
                this->fileLoaded = false;
                this->pending.clear();
                break;
            default:
                break;
            }
        });
    }

    void attach() {
        auto& mpv = MPVCore::instance();
        for (auto& s : this->pending) {
            std::string title = s.lang.empty() ? "Externa" : s.lang;
            // "auto" = add without selecting; the user picks via the + menu.
            mpv.command("sub-add", s.url.c_str(), "auto", title.c_str(), s.lang.c_str());
        }
        this->pending.clear();
    }

    bool inited = false;
    bool fileLoaded = false;
    int requestTicket = 0;
    std::vector<stremio::Subtitle> pending;
};

// Clean text row built in code: name on top, full description below.
class StreamCell : public RecyclingGridItem {
public:
    StreamCell() {
        setFocusable(true); setAxis(brls::Axis::COLUMN); setPadding(12,72,12,22); setCornerRadius(12);
        name=new brls::Label(); name->setFontSize(22); name->setHeight(32); name->setShrink(0);
        name->setSingleLine(true); name->setAutoAnimate(false); name->setWidthPercentage(100); addView(name);
        detail=new brls::Label(); detail->setFontSize(17); detail->setHeight(26); detail->setShrink(0);
        detail->setSingleLine(true); detail->setWidthPercentage(100); detail->setTextColor(stremio_theme::TEXT_DIM); addView(detail);
        registerAction("Detalhes da fonte",brls::BUTTON_X,[this](brls::View*) { if (onDetails) onDetails(); return true; });
    }
    brls::Label* name=nullptr; brls::Label* detail=nullptr;
    bool header=false,expanded=false;
    std::function<void()> onDetails;
    void draw(NVGcontext* vg,float x,float y,float w,float h,brls::Style s,brls::FrameContext* c) override {
        RecyclingGridItem::draw(vg,x,y,w,h,s,c);
        if (!header) return;
        nvgSave(vg); nvgIntersectScissor(vg,x,y,w,h);
        const float cx=x+w-36, cy=y+h/2;
        nvgBeginPath(vg); nvgRoundedRect(vg,cx-16,cy-16,32,32,10);
        nvgFillColor(vg,nvgRGBA(120,225,193,24)); nvgFill(vg);
        nvgBeginPath(vg);
        if (expanded) { nvgMoveTo(vg,cx-5,cy-3); nvgLineTo(vg,cx,cy+3); nvgLineTo(vg,cx+5,cy-3); }
        else { nvgMoveTo(vg,cx-3,cy-5); nvgLineTo(vg,cx+3,cy); nvgLineTo(vg,cx-3,cy+5); }
        nvgStrokeColor(vg,nvgRGB(150,235,208)); nvgStrokeWidth(vg,2.2f);
        nvgLineCap(vg,NVG_ROUND); nvgLineJoin(vg,NVG_ROUND); nvgStroke(vg); nvgRestore(vg);
    }
};
struct SourceRow {
    bool header; size_t group; bool expanded;
    std::string name,detail;
    stremio::Stream stream;
};
class StreamSource : public RecyclingGridDataSource {
public:
    StreamSource(ResumeEntry key,std::vector<SourceRow> rows,std::function<void(size_t)> toggle)
        : key(std::move(key)),rows(std::move(rows)),toggle(std::move(toggle)) {}
    size_t getItemCount() override { return rows.size(); }
    RecyclingGridItem* cellForRow(RecyclingView* recycler,size_t index) override {
        auto* cell=dynamic_cast<StreamCell*>(recycler->dequeueReusableCell("Cell"));
        const auto& row=rows.at(index); cell->header=row.header; cell->expanded=row.expanded;
        cell->onDetails={};
        if (row.header) {
            cell->name->setText(row.name); cell->detail->setText(row.detail);
            cell->name->setTextColor(nvgRGB(160,240,211)); cell->setBackgroundColor(nvgRGB(25,65,70));
        } else {
            const auto raw=row.stream.name+"\n"+row.stream.title+"\n"+row.stream.description;
            const auto favorite=stream_preferences::get().preferredServer;
            const bool preferred=stream_options::preferred(raw,favorite);
            cell->name->setText(stream_options::server(raw,favorite)+(preferred ? " · Preferida" : ""));
            cell->name->setTextColor(preferred ? nvgRGB(110,235,184) : stremio_theme::TEXT);
            cell->setBackgroundColor(preferred ? nvgRGB(20,65,58) : nvgRGBA(255,255,255,16));
            cell->detail->setText(stream_options::quality(raw)+" · "+stream_options::audio(raw));
            cell->onDetails=[raw]() { auto* dialog=new brls::Dialog(raw); dialog->addButton("Fechar",[](){}); dialog->open(); };
        }
        return cell;
    }
    void onItemSelected(brls::Box*,size_t index) override {
        const auto row=rows.at(index);
        if (row.header) { auto action=toggle; action(row.group); return; }
        if (!row.stream.url.empty()) stremio_playback::play(key,row.stream);
    }
    void clearData() override { rows.clear(); }
private:
    ResumeEntry key; std::vector<SourceRow> rows; std::function<void(size_t)> toggle;
};

}  // namespace

void stremio_playback::prepareSubtitles(const std::string& type,const std::string& id) {
    SubLoader::instance().prepare(type,id);
}

StreamPicker::StreamPicker(const std::string& title,const std::vector<stremio::Stream>& streams,const ResumeEntry& key) {
    resumeKey=key;
    setAxis(brls::Axis::COLUMN); setDimensions(brls::Application::contentWidth,brls::Application::contentHeight);
    setPadding(20,40,20,40); setBackgroundColor(nvgRGB(17,23,49)); setFocusable(true); setHideHighlight(true);
    auto* heading=new brls::Label(); heading->setText(title); heading->setFontSize(28);
    heading->setHeight(42); heading->setShrink(0); heading->setSingleLine(true); heading->setAutoAnimate(false); addView(heading);
    auto* intro=new brls::Label(); intro->setText("Escolha um add-on e expanda para ver as fontes disponíveis.");
    intro->setFontSize(17); intro->setHeight(30); intro->setShrink(0); intro->setSingleLine(true); intro->setTextColor(stremio_theme::TEXT_DIM); addView(intro);
    filterButton=new brls::Box(); filterButton->setFocusable(true); filterButton->setPadding(10,20,10,20);
    filterButton->setCornerRadius(12); filterButton->setShrink(0); filterButton->setMarginBottom(14);
    filterLabel=new brls::Label(); filterLabel->setFontSize(18); filterLabel->setHeight(28);
    filterLabel->setSingleLine(true); filterButton->addView(filterLabel); addView(filterButton);
    auto change=[this](brls::View*) { filter=(filter+1)%3; refresh(); return true; };
    filterButton->registerClickAction(change); filterButton->addGestureRecognizer(new brls::TapGestureRecognizer(filterButton));
    registerAction("Filtrar áudio",brls::BUTTON_Y,change);
    recycler=new RecyclingGrid(); recycler->setGrow(1); recycler->setScrollingIndicatorVisible(false);
    recycler->spanCount=1; recycler->isFlowMode=true; recycler->estimatedRowSpace=10;
    recycler->registerCell("Cell",[]()->RecyclingGridItem* { return new StreamCell(); }); addView(recycler);
    stremio_theme::controls(this,{{"A","Expandir / reproduzir"},{"B","Voltar"},{"Y","Filtrar"},{"X","Detalhes"}});
    for (const auto& entry:stremio::VIDEO_ADDONS.enabledEntries()) {
        Group group; group.addon=entry;
        for (const auto& stream:streams) if (stream.sourceAddon==entry.url && !stream.url.empty()) group.streams.push_back(stream);
        if (!group.streams.empty()) group.pending=false;
        groups.push_back(std::move(group));
    }
    refresh();
    for (size_t i=0;i<groups.size();++i) if (groups[i].pending) fetchGroup(i);
    auto guard=alive;
    brls::sync([this,guard]() { if (guard->load()) brls::Application::giveFocus(groups.empty() ? static_cast<brls::View*>(filterButton) : recycler); });
    registerAction("hints/back"_i18n,brls::BUTTON_B,[this](brls::View*) {
        for (auto& token:lookups) if (token) token->store(true);
        brls::Application::popActivity(); return true;
    });
}
StreamPicker::~StreamPicker() { alive->store(false); for (auto& token:lookups) if (token) token->store(true); }
void StreamPicker::fetchGroup(size_t index) {
    auto& group=groups.at(index); group.pending=true; group.error.clear();
    auto guard=alive; const auto target=group.addon.url;
    lookups.push_back(stremio::fetchStreams(resumeKey.streamType,resumeKey.streamId,
        [this,guard,index](stremio::StreamList result,std::string) {
            if (!guard->load()) return;
            auto& group=groups.at(index); group.pending=false; group.streams.clear();
            for (const auto& stream:result.streams) if (!stream.url.empty()) group.streams.push_back(stream);
            refresh();
        },[this,guard,index](const std::string& error) {
            if (!guard->load()) return;
            auto& group=groups.at(index); group.pending=false; group.error=error; refresh();
        },false,target,&stream_worker::search(index)));
}
void StreamPicker::toggle(size_t index) {
    auto& group=groups.at(index);
    if (!group.error.empty()) fetchGroup(index);
    else group.expanded=!group.expanded;
    refresh();
}
void StreamPicker::refresh() {
    bool focused=false; size_t selected=0;
    for (auto* view=brls::Application::getCurrentFocus();view;view=view->getParent()) {
        if (auto* cell=dynamic_cast<RecyclingGridItem*>(view)) selected=cell->getIndex();
        if (view==recycler) focused=true;
    }
    const auto previousKey=selected<rowKeys.size() ? rowKeys[selected] : std::string{};
    if (focused) brls::Application::giveFocus(filterButton);
    const char* labels[]={"Todas as opções","Dublados / áudio duplo","Legendados"};
    filterLabel->setText(std::string(labels[filter])+" · Y filtrar · X detalhes");
    filterButton->setBackgroundColor(filter==0 ? nvgRGB(40,92,154) : nvgRGB(105,64,195));
    std::vector<SourceRow> rows;
    for (size_t i=0;i<groups.size();++i) {
        const auto& group=groups[i]; std::vector<stremio::Stream> filtered;
        for (const auto& stream:group.streams) {
            auto raw=stream.name+"\n"+stream.title+"\n"+stream.description;
            if (filter==1 && !stream_options::dubbed(raw)) continue;
            if (filter==2 && !stream_options::subtitled(raw)) continue;
            filtered.push_back(stream);
        }
        const auto preferred=stream_preferences::get().preferredServer;
        std::stable_sort(filtered.begin(),filtered.end(),[preferred](const auto& a,const auto& b) {
            return stream_options::before(a.name+"\n"+a.title+"\n"+a.description,b.name+"\n"+b.title+"\n"+b.description,preferred);
        });
        const auto status=group.pending ? std::string("Buscando vídeos…") : !group.error.empty()
            ? std::string("Não respondeu · A para tentar novamente") : group.streams.empty()
            ? std::string("Nenhum vídeo disponível neste add-on") : filtered.empty()
            ? std::string("Nenhuma opção neste filtro · Y para alterar")
            : std::to_string(filtered.size())+" opções · A para "+(group.expanded ? "recolher" : "expandir");
        rows.push_back({true,i,group.expanded,group.addon.name,status,{}});
        if (group.expanded) for (const auto& stream:filtered) rows.push_back({false,i,false,"","",stream});
    }
    rowKeys.clear();
    for (const auto& row:rows) rowKeys.push_back(groups[row.group].addon.url+(row.header ? "#header" : "#"+row.stream.url));
    auto matching=std::find(rowKeys.begin(),rowKeys.end(),previousKey);
    if (matching!=rowKeys.end()) selected=std::distance(rowKeys.begin(),matching);
    const auto count=rows.size();
    if (rows.empty()) recycler->setEmpty("Nenhum add-on ativado. Ative um em Configurações > Add-ons de vídeo.");
    else recycler->setDataSource(new StreamSource(resumeKey,std::move(rows),[this](size_t index){toggle(index);}));
    if (focused && count) { recycler->selectRowAt(std::min(selected,count-1),false); brls::Application::giveFocus(recycler); }
}
