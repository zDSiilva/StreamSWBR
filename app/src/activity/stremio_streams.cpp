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
        this->setFocusable(true);
        this->setAxis(brls::Axis::COLUMN);
        this->setPadding(16, 24, 16, 24);
        this->setCornerRadius(12);
        this->setBackgroundColor(nvgRGBA(255, 255, 255, 16));

        this->name = new brls::Label();
        this->name->setFontSize(24);
        this->name->setHeight(34);
        this->name->setMarginBottom(7);
        this->name->setShrink(0);
        this->name->setSingleLine(true);
        this->name->setAutoAnimate(false);
        this->name->setTextColor(stremio_theme::TEXT);
        this->addView(this->name);

        this->detail = new brls::Label();
        this->detail->setFontSize(18);
        this->detail->setHeight(26);
        this->detail->setShrink(0);
        this->detail->setSingleLine(true);
        this->detail->setTextColor(stremio_theme::TEXT_DIM);
        this->addView(this->detail);
        this->registerAction("Detalhes da fonte", brls::BUTTON_X, [this](brls::View*) {
            if (onDetails) onDetails();
            return true;
        });
    }

    brls::Label* name = nullptr;
    brls::Label* detail = nullptr;
    std::function<void()> onDetails;
};

class StreamSource : public RecyclingGridDataSource {
public:
    StreamSource(const ResumeEntry& key, const std::vector<stremio::Stream>& s)
        : key(key), list(std::move(s)) {}

    size_t getItemCount() override { return this->list.size(); }

    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override {
        StreamCell* cell = dynamic_cast<StreamCell*>(recycler->dequeueReusableCell("Cell"));
        auto& s = this->list.at(index);

        const std::string raw = s.name + "\n" + s.title + "\n" + s.description;
        auto favorite = stream_preferences::get().preferredServer;
        bool preferred = stream_options::preferred(raw, favorite);
        cell->name->setText(stream_options::server(raw, favorite) + (preferred ? "  ★ Preferido" : ""));
        cell->detail->setText(stream_options::quality(raw) + "  ·  " + stream_options::audio(raw) +
            "  ·  Opção " + std::to_string(index + 1));
        cell->onDetails = [raw]() {
            auto* dialog = new brls::Dialog(raw);
            dialog->addButton("Fechar", []() {});
            dialog->open();
        };
        return cell;
    }

    void onItemSelected(brls::Box* recycler, size_t index) override {
        auto s = this->list.at(index);
        if (s.url.empty()) {
            brls::Application::notify("Este vídeo não tem endereço de reprodução");
            return;
        }
        // Remember what we're playing so the tracker can resume/record by id.
        ResumeTracker::instance().setCurrent(this->key);
        // Ask the subtitles addon (if configured) for subs for this title.
        SubLoader::instance().prepare(this->key.streamType, this->key.streamId);
        auto preferences = stream_preferences::get();
        MPVCore::instance().command("set", "alang", preferences.audio.c_str());
        MPVCore::instance().command("set", "slang", preferences.subtitles == "off" ? "" : preferences.subtitles.c_str());
        MPVCore::instance().command("set", "sid", preferences.subtitles == "off" ? "no" : "auto");
        // Show the media title (movie name / "Series · S1E5 · Episode") on the
        // player OSD, not the raw stream/source name.
        std::string title = this->key.name.empty() ? s.name : this->key.name;
        RemoteView::play(s.url, title);
    }

    void clearData() override { this->list.clear(); }

private:
    ResumeEntry key;
    std::vector<stremio::Stream> list;
};

}  // namespace

StreamPicker::StreamPicker(
    const std::string& title, const std::vector<stremio::Stream>& streams, const ResumeEntry& resumeKey) {
    this->resumeKey = resumeKey;
    brls::Logger::debug("StreamPicker: {} streams", streams.size());
    this->setAxis(brls::Axis::COLUMN);
    this->setDimensions(brls::Application::contentWidth, brls::Application::contentHeight);
    this->setBackgroundColor(nvgRGB(17, 23, 49));
    this->setPadding(20, 40, 20, 40);
    // Hold focus here (hidden highlight) while the list loads / when empty,
    // so the outline can't float over the covered detail screen.
    this->setFocusable(true);
    this->setHideHighlight(true);

    auto* heading = new brls::Label();
    heading->setText(title); heading->setFontSize(28); heading->setSingleLine(true);
    heading->setHeight(42); heading->setShrink(0);
    heading->setAutoAnimate(false); heading->setTextColor(stremio_theme::TEXT);
    heading->setMarginBottom(12); this->addView(heading);
    auto* filters = new brls::Box();
    this->filterButton = filters;
    filters->setFocusable(true); filters->setPadding(12, 20, 12, 20); filters->setCornerRadius(12);
    filters->setShrink(0);
    filters->setMarginBottom(20); filters->setBackgroundColor(nvgRGB(40, 92, 154));
    this->filterLabel = new brls::Label(); this->filterLabel->setFontSize(19);
    this->filterLabel->setHeight(30); this->filterLabel->setSingleLine(true);
    this->filterLabel->setTextColor(stremio_theme::TEXT); filters->addView(this->filterLabel);
    auto changeFilter = [this](brls::View*) {
        this->filter = (this->filter + 1) % 3; this->refresh(); return true;
    };
    filters->registerClickAction(changeFilter);
    filters->addGestureRecognizer(new brls::TapGestureRecognizer(filters));
    this->addView(filters);
    this->registerAction("Filtrar áudio", brls::BUTTON_Y, changeFilter);

    this->recycler = new RecyclingGrid();
    this->recycler->setGrow(1.0f);
    this->recycler->setScrollingIndicatorVisible(false);
    this->recycler->spanCount = 1;
    this->recycler->isFlowMode = true;
    this->recycler->estimatedRowSpace = 10;
    this->recycler->registerCell("Cell", []() -> RecyclingGridItem* { return new StreamCell(); });
    this->addView(this->recycler);
    stremio_theme::controls(this, {{"A","Reproduzir"},{"B","Voltar"},{"Y","Filtrar"},{"X","Detalhes"}});

    // Addons (e.g. AIOStreams) may return non-playable info entries alongside
    // real streams — "Removal Reasons", "no results", etc. — with no URL.
    // Keep only playable streams so the user can't select a dead entry.
    for (auto& s : streams)
        if (!s.url.empty()) this->streams.push_back(s);
    this->refresh();
    // Defer focus until after the activity is on screen.
    brls::sync([this]() { brls::Application::giveFocus(this); });
    brls::sync([this]() {
        brls::Application::giveFocus(this->streams.empty() ? static_cast<brls::View*>(this->filterButton) : this->recycler);
    });

    this->registerAction("hints/back"_i18n, brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
}

void StreamPicker::refresh() {
    bool focused = false;
    for (auto* view = brls::Application::getCurrentFocus(); view; view = view->getParent())
        if (view == this) { focused = true; break; }
    // Recycled rows must not retain the global focus while the filter changes.
    if (focused) brls::Application::giveFocus(this->filterButton);
    const char* labels[] = {"Todas as opções", "Dublados / áudio duplo", "Legendados"};
    std::vector<stremio::Stream> filtered;
    for (const auto& stream : this->streams) {
        auto raw = stream.name + "\n" + stream.title + "\n" + stream.description;
        if (filter == 1 && !stream_options::dubbed(raw)) continue;
        if (filter == 2 && !stream_options::subtitled(raw)) continue;
        filtered.push_back(stream);
    }
    auto preferred = stream_preferences::get().preferredServer;
    std::stable_sort(filtered.begin(), filtered.end(), [preferred](const stremio::Stream& a, const stremio::Stream& b) {
        auto isPreferred = [&preferred](const stremio::Stream& s) {
            return stream_options::preferred(s.name + "\n" + s.title + "\n" + s.description, preferred);
        };
        return isPreferred(a) && !isPreferred(b);
    });
    this->filterLabel->setText(std::string(labels[filter]) + " · " + std::to_string(filtered.size()) +
        " opções · Y filtrar · X detalhes");
    this->filterButton->setBackgroundColor(filter == 0 ? nvgRGB(40, 92, 154) : nvgRGB(105, 64, 195));
    if (filtered.empty()) this->recycler->setEmpty(this->streams.empty() ?
        "Nenhum vídeo compatível. Verifique o complemento." : "Nenhuma opção neste filtro. Pressione Y para ver todas.");
    else this->recycler->setDataSource(new StreamSource(this->resumeKey, filtered));
    if (focused && !filtered.empty()) brls::Application::giveFocus(this->recycler);
}
