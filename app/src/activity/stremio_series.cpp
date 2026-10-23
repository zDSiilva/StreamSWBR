/*
    Stremio series + season screens -- implementation.
*/
#include "activity/stremio_series.hpp"
#include "activity/stremio_streams.hpp"
#include "activity/stremio_resume.hpp"
#include "activity/stremio_playback.hpp"
#include "activity/watched_store.hpp"
#include "view/recycling_grid.hpp"
#include "view/video_card.hpp"
#include "view/svg_image.hpp"
#include "utils/image.hpp"
#include "api/stremio.hpp"
#include "view/stremio_controls.hpp"

#include <map>

using namespace brls::literals;

namespace {

// ---- Season list cell (compact icon + name + episode count) --------------
class SeasonCell : public RecyclingGridItem {
public:
    SeasonCell() {
        setAxis(brls::Axis::COLUMN); setFocusable(true); setPadding(18,24,18,24);
        setCornerRadius(14); setBackgroundColor(nvgRGB(27,39,65));
        name=new brls::Label(); name->setFontSize(24); name->setHeight(34);
        name->setSingleLine(true); name->setWidthPercentage(100);
        name->setShrink(0); name->setTextColor(stremio_theme::TEXT); addView(name);
        detail=new brls::Label(); detail->setFontSize(17); detail->setHeight(26);
        detail->setSingleLine(true); detail->setWidthPercentage(100);
        detail->setShrink(0); detail->setTextColor(stremio_theme::TEXT_DIM); addView(detail);
    }
    brls::Label* name=nullptr;
    brls::Label* detail=nullptr;
    std::set<std::string> episodeIds;
    void draw(NVGcontext* vg,float x,float y,float w,float h,brls::Style style,brls::FrameContext* ctx) override {
        size_t done=0; for (const auto& id:episodeIds) if (watched::store().contains(id)) ++done;
        bool complete=!episodeIds.empty() && done==episodeIds.size();
        auto text=complete ? std::string("Temporada concluída") : fmt::format("{} de {} episódios assistidos",done,episodeIds.size());
        if (detail->getFullText()!=text) detail->setText(text);
        detail->setTextColor(complete ? nvgRGB(120,220,175) : stremio_theme::TEXT_DIM);
        RecyclingGridItem::draw(vg,x,y,w,h,style,ctx);
        nvgSave(vg); nvgIntersectScissor(vg,x+20,y+4,w-40,h-8);
        nvgBeginPath(vg); nvgRoundedRect(vg,x+24,y+h-13,w-48,3,1.5f);
        nvgFillColor(vg,nvgRGBA(120,220,175,35)); nvgFill(vg);
        if (done && !episodeIds.empty()) {
            nvgBeginPath(vg); nvgRoundedRect(vg,x+24,y+h-13,(w-48)*done/episodeIds.size(),3,1.5f);
            nvgFillColor(vg,nvgRGB(100,205,155)); nvgFill(vg);
        }
        if (complete) {
            nvgBeginPath(vg); nvgMoveTo(vg,x+w-48,y+30); nvgLineTo(vg,x+w-42,y+36); nvgLineTo(vg,x+w-31,y+23);
            nvgStrokeWidth(vg,2); nvgStrokeColor(vg,nvgRGB(120,220,175)); nvgStroke(vg);
        }
        nvgRestore(vg);
    }
};

struct SeasonEntry {
    std::string label;
    std::vector<stremio::Video> episodes;
};

class SeasonSource : public RecyclingGridDataSource {
public:
    SeasonSource(std::string seriesName, std::string background, std::vector<SeasonEntry> s)
        : seriesName(std::move(seriesName)), background(std::move(background)), list(std::move(s)) {}

    size_t getItemCount() override { return this->list.size(); }

    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override {
        SeasonCell* cell = dynamic_cast<SeasonCell*>(recycler->dequeueReusableCell("Cell"));
        auto& s = this->list.at(index);
        cell->name->setText(s.label);
        cell->episodeIds.clear(); for (const auto& episode:s.episodes) if (!episode.id.empty()) cell->episodeIds.insert(episode.id);
        return cell;
    }

    void onItemSelected(brls::Box* recycler, size_t index) override {
        auto& s = this->list.at(index);
        brls::Application::pushActivity(
            new brls::Activity(new StremioSeason(this->seriesName, s.label, s.episodes, this->background)));
    }

    void clearData() override { this->list.clear(); }

private:
    std::string seriesName;
    std::string background;
    std::vector<SeasonEntry> list;
};

// ---- Episode list cell (wide card with thumbnail) ------------------------
class EpisodeCell : public BaseCardCell {
public:
    EpisodeCell() { this->inflateFromXMLRes("xml/view/episode_card.xml"); }
    std::string episodeId;
    void draw(NVGcontext* vg,float x,float y,float w,float h,brls::Style style,brls::FrameContext* ctx) override {
        BaseCardCell::draw(vg,x,y,w,h,style,ctx);
        if (!watched::store().contains(episodeId)) return;
        // Draw the check directly; it never depends on a font glyph or title width.
        nvgSave(vg); nvgIntersectScissor(vg,x,y,w,h);
        nvgBeginPath(vg); nvgCircle(vg,x+213,y+25,12); nvgFillColor(vg,nvgRGBA(19,85,60,245)); nvgFill(vg);
        nvgBeginPath(vg); nvgMoveTo(vg,x+207,y+25); nvgLineTo(vg,x+211,y+29); nvgLineTo(vg,x+219,y+20);
        nvgStrokeWidth(vg,2); nvgStrokeColor(vg,nvgRGB(160,245,201)); nvgStroke(vg); nvgRestore(vg);
    }

    BRLS_BIND(brls::Label, labelName, "episode/card/name");
    BRLS_BIND(brls::Label, labelOverview, "episode/card/overview");
};

class EpisodeSource : public RecyclingGridDataSource {
public:
    EpisodeSource(std::string seriesName, std::string background, const std::vector<stremio::Video>& v)
        : seriesName(std::move(seriesName)), background(std::move(background)), list(std::move(v)) {}

    size_t getItemCount() override { return this->list.size(); }

    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override {
        EpisodeCell* cell = dynamic_cast<EpisodeCell*>(recycler->dequeueReusableCell("Cell"));
        auto& v = this->list.at(index);
        cell->episodeId=v.id;

        cell->labelName->setText(fmt::format("Episódio {} · {}", v.episode, v.name));
        // Prefix the air date (YYYY-MM-DD) to the overview when available.
        std::string date = v.released.size() >= 10 ? v.released.substr(0, 10) : "";
        std::string ov = v.overview;
        if (!date.empty()) ov = ov.empty() ? date : (date + "  ·  " + ov);
        cell->labelOverview->setText(ov);
        cell->badgeTopRight->setVisibility(brls::Visibility::GONE);
        cell->rectProgress->getParent()->setVisibility(brls::Visibility::GONE);

        // Newly-aired episodes often have no still yet (metahub 404s); fall
        // back to the series backdrop instead of the grey placeholder.
        if (!v.thumbnail.empty())
            Image::with(cell->picture, v.thumbnail, this->background);
        else if (!this->background.empty())
            Image::with(cell->picture, this->background);

        return cell;
    }

    void onItemSelected(brls::Box* recycler, size_t index) override {
        auto v = this->list.at(index);
        if (stremio::STREAM_ADDON.empty()) {
            brls::Application::notify("Configure o complemento primeiro (pressione − na tela inicial)");
            return;
        }
        brls::Application::notify("Buscando vídeos…");
        stremio_playback::interruptResume();

        // Player OSD / resume label: include the series name so it reads
        // "Series · S1E5 · Episode name" instead of just the episode.
        std::string label = this->seriesName.empty()
            ? fmt::format("S{}E{} · {}", v.season, v.episode, v.name)
            : fmt::format("{} · S{}E{} · {}", this->seriesName, v.season, v.episode, v.name);
        std::string vid = v.id, thumb = v.thumbnail;
        stremio::fetchStreams("series", vid,
            [label, vid, thumb](stremio::StreamList r, std::string usedType) {
                ResumeEntry key{usedType, vid, label, thumb};
                brls::Application::pushActivity(new brls::Activity(new StreamPicker(label, r.streams, key)));
            },
            [label,vid,thumb](const std::string& e) { brls::Application::notify("Erro ao buscar vídeos: " + e); ResumeEntry key{"series",vid,label,thumb}; brls::Application::pushActivity(new brls::Activity(new StreamPicker(label,{},key))); });
    }

    void clearData() override { this->list.clear(); }

private:
    std::string seriesName;
    std::string background;
    std::vector<stremio::Video> list;
};

}  // namespace

// ---- StremioSeries: lists seasons ----------------------------------------
void StremioSeries::init() {
    this->setAxis(brls::Axis::COLUMN);
    this->setDimensions(brls::Application::contentWidth, brls::Application::contentHeight);
    this->setBackgroundColor(brls::Application::getTheme()["brls/background"]);
    this->setPadding(20, 40, 20, 40);

    this->recycler = new RecyclingGrid();
    this->recycler->setGrow(1.0f);
    this->recycler->setScrollingIndicatorVisible(false);
    this->recycler->spanCount = 1;
    auto* title=new brls::Label(); title->setText("Temporadas"); title->setFontSize(32);
    title->setHeight(46); title->setShrink(0); title->setMarginBottom(16); addView(title);
    loadingHint=new brls::Label(); loadingHint->setText("Carregando temporadas e episódios…");
    loadingHint->setFontSize(14); loadingHint->setHeight(26); loadingHint->setShrink(0);
    loadingHint->setTextColor(stremio_theme::TEXT_DIM); loadingHint->setVisibility(brls::Visibility::GONE);
    addView(loadingHint);
    this->recycler->estimatedRowHeight = 100;
    this->recycler->estimatedRowSpace = 14;
    this->recycler->registerCell("Cell", []() -> RecyclingGridItem* { return new SeasonCell(); });
    this->addView(this->recycler);
    stremio_theme::controls(this, {{"A","Abrir temporada"},{"B","Voltar"}});
    brls::sync([this]() { brls::Application::giveFocus(this->recycler); });

    this->registerAction("hints/back"_i18n, brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
}

void StremioSeries::setSeasons(
    const std::string& name, const std::vector<stremio::Video>& videos, const std::string& background) {
    loadingHint->setVisibility(brls::Visibility::GONE);
    if (videos.empty()) {
        this->recycler->setError("Nenhum episódio encontrado");
        return;
    }
    // Group episodes by season (std::map keeps seasons in order).
    std::map<int, std::vector<stremio::Video>> bySeason;
    for (auto& v : videos) bySeason[v.season].push_back(v);

    std::vector<SeasonEntry> seasons;
    for (auto& kv : bySeason) {
        if (kv.first == 0) continue;  // skip specials (season 0)
        seasons.push_back({fmt::format("Temporada {}", kv.first), kv.second});
    }
    if (seasons.empty()) {
        this->recycler->setError("Nenhum episódio encontrado");
        return;
    }
    this->recycler->setDataSource(new SeasonSource(name, background, std::move(seasons)));
    brls::Application::giveFocus(this->recycler);
}

StremioSeries::StremioSeries(const stremio::Meta& series) {
    brls::Logger::debug("StremioSeries: create {}", series.id);
    this->init();
    this->recycler->showSkeleton();
    loadingHint->setVisibility(brls::Visibility::VISIBLE);

    std::string seriesName = series.name;
    ASYNC_RETAIN
    stremio::getJSON<stremio::MetaResult>(
        [ASYNC_TOKEN, seriesName](stremio::MetaResult r) {
            ASYNC_RELEASE
            // Prefer the (English) name from the meta response; fall back to the
            // catalog name we were opened with.
            this->setSeasons(r.meta.name.empty() ? seriesName : r.meta.name, r.meta.videos, r.meta.background);
        },
        [ASYNC_TOKEN](const std::string& e) {
            ASYNC_RELEASE
            this->loadingHint->setVisibility(brls::Visibility::GONE);
            this->recycler->setError(e);
        },
        stremio::CINEMETA + "/meta/series/" + series.id + ".json",true);
}

StremioSeries::StremioSeries(
    const std::string& name, const std::vector<stremio::Video>& videos, const std::string& background) {
    brls::Logger::debug("StremioSeries: create from {} prefetched episodes", videos.size());
    this->init();
    this->setSeasons(name, videos, background);
}

// ---- StremioSeason: lists episodes of one season -------------------------
StremioSeason::StremioSeason(const std::string& seriesName, const std::string& title,
    const std::vector<stremio::Video>& episodes, const std::string& background) {
    brls::Logger::debug("StremioSeason: {} ({} eps)", title, episodes.size());
    this->setAxis(brls::Axis::COLUMN);
    this->setFocusable(true);
    this->setHideHighlight(true);
    this->setDimensions(brls::Application::contentWidth, brls::Application::contentHeight);
    this->setBackgroundColor(brls::Application::getTheme()["brls/background"]);
    this->setPadding(20, 40, 20, 40);

    auto* titleLabel=new brls::Label(); titleLabel->setText(seriesName+" · "+title);
    titleLabel->setFontSize(28); titleLabel->setHeight(44); titleLabel->setShrink(0);
    titleLabel->setSingleLine(true); titleLabel->setMarginBottom(14); addView(titleLabel);

    this->recycler = new RecyclingGrid();
    this->recycler->setGrow(1.0f);
    this->recycler->setScrollingIndicatorVisible(false);
    this->recycler->spanCount = 1;
    this->recycler->isFlowMode = true;
    this->recycler->estimatedRowSpace = 10;
    this->recycler->registerCell("Cell", []() -> RecyclingGridItem* { return new EpisodeCell(); });
    this->addView(this->recycler);
    stremio_theme::controls(this, {{"A","Escolher vídeo"},{"B","Temporadas"}});

    this->registerAction("hints/back"_i18n, brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    auto localized = episodes;
    for (auto& video : localized) {
        video.name = "Episódio " + std::to_string(video.episode);
        video.overview = "Sinopse em português indisponível.";
    }
    this->recycler->setDataSource(new EpisodeSource(seriesName, background, localized));
    brls::sync([this]() { brls::Application::giveFocus(this->recycler); });
    if (!episodes.empty()) {
        auto id = episodes.front().id.substr(0, episodes.front().id.find(':'));
        auto seasonNumber = episodes.front().season;
        ASYNC_RETAIN
        portuguese_metadata::season(id, seasonNumber,
            [ASYNC_TOKEN, localized, seriesName, background](nlohmann::json data) mutable {
                ASYNC_RELEASE
                if (!data.contains("episodes") || !data["episodes"].is_array()) return;
                for (auto& video : localized) {
                    for (const auto& entry : data["episodes"]) {
                        if (stremio::jint(entry, "episode_number") != video.episode) continue;
                        auto name = stremio::jstr(entry, "name"), overview = stremio::jstr(entry, "overview");
                        if (!name.empty()) video.name = name;
                        if (!overview.empty()) video.overview = overview;
                        break;
                    }
                }
                bool focused = false;
                size_t index = 0;
                for (auto* view = brls::Application::getCurrentFocus(); view; view = view->getParent()) {
                    if (auto* cell = dynamic_cast<RecyclingGridItem*>(view)) index = cell->getIndex();
                    if (view == this) { focused = true; break; }
                }
                if (focused) brls::Application::giveFocus(this);
                this->recycler->setDataSource(new EpisodeSource(seriesName, background, localized));
                if (focused) {
                    this->recycler->selectRowAt(index, false);
                    brls::Application::giveFocus(this->recycler);
                }
            });
    }
}
