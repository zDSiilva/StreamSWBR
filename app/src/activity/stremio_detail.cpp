/*
    Stremio movie detail screen -- implementation.

    Built programmatically (like the home screen) to avoid the XML-inflation
    ghosting issue. Layout: poster on the left; title, meta line, Watch button,
    description, cast and director in a column on the right.
*/
#include "activity/stremio_detail.hpp"
#include "activity/stremio_series.hpp"
#include "activity/stremio_streams.hpp"
#include "activity/stremio_favourites.hpp"
#include "activity/stremio_resume.hpp"
#include "activity/stremio_playback.hpp"
#include "view/stremio_theme.hpp"
#include "view/stremio_controls.hpp"
#include "utils/image.hpp"
#include "api/stremio.hpp"

#include <set>

using namespace brls::literals;

namespace {

// Soft palette shared with the rest of the app.
const NVGcolor COL_TEXT   = stremio_theme::TEXT;      // soft white
const NVGcolor COL_DIM    = stremio_theme::TEXT_DIM;  // secondary text
const NVGcolor COL_ACCENT = stremio_theme::ACCENT_HI; // light Stremio purple (meta line)

// Fullscreen scrollable description: everything else stays still; the text
// scrolls with up/down. Opened by the blue "Ver mais" under the description.
class DescriptionScreen : public brls::Box {
public:
    DescriptionScreen(const std::string& title, const std::string& text) {
        this->setAxis(brls::Axis::COLUMN);
        this->setDimensions(brls::Application::contentWidth, brls::Application::contentHeight);
        this->setPadding(40, 120, 40, 120);

        auto* head = new brls::Label();
        head->setText(title);
        head->setFontSize(34);
        head->setWidthPercentage(100); head->setSingleLine(false); head->setShrink(0);
        head->setTextColor(stremio_theme::TEXT);
        head->setMarginBottom(20);
        this->addView(head);

        auto* scroll = new brls::ScrollingFrame();
        scroll->setGrow(1.0f);
        scroll->setHideHighlight(true);  // the frame itself takes focus to scroll
        auto* body = new brls::Box();
        body->setAxis(brls::Axis::COLUMN);
        auto* lbl = new brls::Label();
        lbl->setText(text);
        lbl->setFontSize(24);
        lbl->setTextColor(stremio_theme::TEXT_DIM);
        lbl->setIsWrapping(true);
        lbl->setSingleLine(false); lbl->setWidthPercentage(100); lbl->setLineHeight(1.35f); lbl->setShrink(0);
        body->addView(lbl);
        scroll->setContentView(body);
        this->addView(scroll);

        this->registerAction("hints/back"_i18n, brls::BUTTON_B, [](brls::View*) {
            brls::Application::popActivity();
            return true;
        });
        brls::sync([scroll]() { brls::Application::giveFocus(scroll); });
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
        brls::FrameContext* ctx) override {
        stremio_theme::drawOceanBackground(vg, x, y, width, height);
        brls::Box::draw(vg, x, y, width, height, style, ctx);
    }
};

// Join a string list: {"A","B"} -> "A, B".
std::string join(const std::vector<std::string>& v, const char* sep = ", ") {
    std::string out;
    for (auto& s : v) {
        if (s.empty()) continue;
        if (!out.empty()) out += sep;
        out += s;
    }
    return out;
}

}  // namespace

StremioDetail::StremioDetail(const stremio::Meta& item) : item(item) {
    brls::Logger::debug("StremioDetail: {}", item.id);
    this->setAxis(brls::Axis::COLUMN);
    this->setDimensions(brls::Application::contentWidth, brls::Application::contentHeight);
    // Background is the ocean gradient painted in draw().
    this->setPadding(40, 60, 40, 60);

    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    row->setGrow(1.0f);
    this->addView(row);

    // ---- Poster (left) ----------------------------------------------------
    this->poster = new brls::Image();
    this->poster->setDimensions(340, 510);
    this->poster->setShrink(0);
    this->poster->setCornerRadius(10);
    this->poster->setScalingType(brls::ImageScalingType::FILL);
    // The texture is shared with the home-grid cell via the URL-keyed
    // TextureCache (refcounted). The view must never delete it itself, or the
    // grid poster goes white when this screen is popped. Image::with only sets
    // this on the download path, not on a cache hit, so set it explicitly.
    this->poster->setFreeTexture(false);
    row->addView(this->poster);
    std::string posterSrc = stremio::posterUrl(item.id, item.poster);
    if (!posterSrc.empty()) Image::with(this->poster, posterSrc);

    // ---- Info column (right) ----------------------------------------------
    auto* infoScroll = new brls::ScrollingFrame();
    infoScroll->setGrow(1); infoScroll->setShrink(1); infoScroll->setMarginLeft(40);
    infoScroll->setScrollingIndicatorVisible(true);
    row->addView(infoScroll);
    auto* info = new brls::Box();
    info->setAxis(brls::Axis::COLUMN);
    info->setWidthPercentage(100); info->setPaddingRight(18); info->setPaddingBottom(30);
    infoScroll->setContentView(info);

    this->labelTitle = new brls::Label();
    this->labelTitle->setText(portuguese_metadata::cachedTitle(item.id,item.type,item.name));
    this->labelTitle->setFontSize(34);
    this->labelTitle->setSingleLine(false); this->labelTitle->setWidthPercentage(100);
    this->labelTitle->setShrink(0); this->labelTitle->setLineHeight(1.15f);
    this->labelTitle->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    this->labelTitle->setTextColor(COL_TEXT);
    this->labelTitle->setIsWrapping(true);
    info->addView(this->labelTitle);

    this->labelMeta = new brls::Label();
    this->labelMeta->setText(item.year);
    this->labelMeta->setFontSize(18);
    this->labelMeta->setWidthPercentage(100); this->labelMeta->setSingleLine(false); this->labelMeta->setShrink(0);
    this->labelMeta->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    this->labelMeta->setTextColor(COL_ACCENT);
    this->labelMeta->setMarginTop(10);
    info->addView(this->labelMeta);

    this->labelGenres = new brls::Label();
    this->labelGenres->setFontSize(17);
    this->labelGenres->setWidthPercentage(100); this->labelGenres->setSingleLine(false); this->labelGenres->setShrink(0);
    this->labelGenres->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    this->labelGenres->setTextColor(COL_DIM);
    this->labelGenres->setMarginTop(6);
    this->labelGenres->setVisibility(brls::Visibility::GONE);
    info->addView(this->labelGenres);

    // Watch button: a focusable pill, same construction style as StreamCell.
    auto* btnRow = new brls::Box();
    btnRow->setAxis(brls::Axis::ROW);
    btnRow->setMarginTop(22);
    btnRow->setShrink(0);
    info->addView(btnRow);

    this->btnWatch = new brls::Box();
    this->btnWatch->setFocusable(true);
    this->btnWatch->setAxis(brls::Axis::ROW);
    this->btnWatch->setAlignItems(brls::AlignItems::CENTER);
    this->btnWatch->setPadding(14, 40, 14, 40);
    this->btnWatch->setCornerRadius(8);
    this->btnWatch->setBackgroundColor(stremio_theme::ACCENT);  // Stremio purple
    // Keep the purple visible while focused: the borealis highlight backdrop
    // is painted over a view's own background and would cover it. Match the
    // focus outline to the pill's rounded shape.
    this->btnWatch->setHideHighlightBackground(true);
    this->btnWatch->setHighlightCornerRadius(11);
    this->btnWatch->setHighlightPadding(3);
    this->btnLabel = new brls::Label();
    // ▶ is in the Switch system font; ☰ is not (renders as a crossed box).
    this->btnLabel->setText(item.type == "series" ? "▶  Episódios" : "▶  Assistir");
    this->btnLabel->setFontSize(24);
    this->btnLabel->setTextColor(COL_TEXT);
    this->btnWatch->addView(this->btnLabel);
    this->btnWatch->registerClickAction([this](brls::View*) {
        this->onAction();
        return true;
    });
    this->btnWatch->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnWatch));
    btnRow->addView(this->btnWatch);

    auto* synopsisHead = new brls::Label(); synopsisHead->setText("Sinopse");
    synopsisHead->setFontSize(21); synopsisHead->setHeight(30); synopsisHead->setShrink(0);
    synopsisHead->setMarginTop(24); synopsisHead->setMarginBottom(8); synopsisHead->setTextColor(COL_TEXT);
    synopsisHead->setHorizontalAlign(brls::HorizontalAlign::LEFT); info->addView(synopsisHead);
    this->labelDesc = new brls::Label();
    this->labelDesc->setText("Carregando sinopse…");
    this->labelDesc->setFontSize(20);
    this->labelDesc->setWidthPercentage(100); this->labelDesc->setSingleLine(false); this->labelDesc->setShrink(0);
    this->labelDesc->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    this->labelDesc->setLineHeight(1.4f);
    this->labelDesc->setTextColor(COL_DIM);
    this->labelDesc->setIsWrapping(true);
    this->labelDesc->setMarginTop(0);
    info->addView(this->labelDesc);

    // Blue "Ver mais" pill — only shown when the description was truncated.
    this->btnMore = new brls::Box();
    this->btnMore->setFocusable(true);
    this->btnMore->setAxis(brls::Axis::ROW);
    this->btnMore->setPadding(6, 14, 6, 14);
    this->btnMore->setCornerRadius(6);
    this->btnMore->setMarginTop(8);
    this->btnMore->setMaxWidth(160);
    this->btnMore->setHideHighlightBackground(true);
    this->btnMore->setHighlightCornerRadius(8);
    this->btnMore->setVisibility(brls::Visibility::GONE);
    this->btnMore->setShrink(0);
    auto* moreLbl = new brls::Label();
    moreLbl->setText("Ver mais");
    moreLbl->setFontSize(20);
    moreLbl->setTextColor(nvgRGB(96, 150, 255));  // link blue
    this->btnMore->addView(moreLbl);
    this->btnMore->registerClickAction([this](brls::View*) {
        brls::Application::pushActivity(
            new brls::Activity(new DescriptionScreen(this->item.name, this->fullDesc)),
            brls::TransitionAnimation::NONE);
        return true;
    });
    this->btnMore->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnMore));
    info->addView(this->btnMore);

    this->labelCastHead = new brls::Label();
    this->labelCastHead->setText("Elenco");
    this->labelCastHead->setFontSize(24);
    this->labelCastHead->setHeight(32); this->labelCastHead->setShrink(0);
    this->labelCastHead->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    this->labelCastHead->setTextColor(COL_TEXT);
    this->labelCastHead->setMarginTop(26);
    this->labelCastHead->setVisibility(brls::Visibility::GONE);
    info->addView(this->labelCastHead);

    this->labelCast = new brls::Label();
    this->labelCast->setFontSize(18);
    this->labelCast->setSingleLine(false); this->labelCast->setWidthPercentage(100); this->labelCast->setShrink(0);
    this->labelCast->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    this->labelCast->setLineHeight(1.35f);
    this->labelCast->setTextColor(COL_DIM);
    this->labelCast->setIsWrapping(true);
    this->labelCast->setMarginTop(8);
    this->labelCast->setVisibility(brls::Visibility::GONE);
    info->addView(this->labelCast);

    this->labelDirector = new brls::Label();
    this->labelDirector->setFontSize(18);
    this->labelDirector->setSingleLine(false); this->labelDirector->setWidthPercentage(100); this->labelDirector->setShrink(0);
    this->labelDirector->setHorizontalAlign(brls::HorizontalAlign::LEFT);
    this->labelDirector->setTextColor(COL_DIM);
    this->labelDirector->setMarginTop(14);
    this->labelDirector->setVisibility(brls::Visibility::GONE);
    info->addView(this->labelDirector);
    stremio_theme::controls(this, {{"A","Assistir / episódios"},{"X","Favoritos"},{"B","Voltar"}});

    // ---- Actions -----------------------------------------------------------
    this->registerAction("hints/back"_i18n, brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    // X toggles library membership here too, matching the poster grids.
    this->registerAction("Favoritos", brls::BUTTON_X, [this](brls::View*) {
        bool nowFav = Favourites::instance().toggle(this->item);
        brls::Application::notify(nowFav ? "Adicionado aos favoritos" : "Removido dos favoritos");
        return true;
    });

    // Something must hold focus so A/B work immediately.
    brls::sync([this]() { brls::Application::giveFocus(this->btnWatch); });

    // ---- Fetch full metadata (Cinemeta; Kitsu addon for kitsu: ids) --------
    const std::string& metaBase = item.id.rfind("kitsu:", 0) == 0 ? stremio::KITSU : stremio::CINEMETA;
    ASYNC_RETAIN
    stremio::getJSON<stremio::MetaResult>(
        [ASYNC_TOKEN](stremio::MetaResult r) {
            ASYNC_RELEASE
            this->applyMeta(r.meta);
        },
        [ASYNC_TOKEN](const std::string&) {
            ASYNC_RELEASE
            this->labelDesc->setText("Nenhuma informação disponível.");
        },
        metaBase + "/meta/" + item.type + "/" + item.id + ".json",true);
}

// Release our reference on the shared poster texture (and abort any in-flight
// request) — same contract as BaseCardCell.
StremioDetail::~StremioDetail() { Image::cancel(this->poster); }

void StremioDetail::draw(
    NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    stremio_theme::drawOceanBackground(vg, x, y, width, height);
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

void StremioDetail::applyMeta(const stremio::MetaDetail& meta) {
    // Prefer the localized full metadata name — for the title AND
    // for the resume/OSD label used when Watch is pressed.
    if (!meta.name.empty()) {
        this->labelTitle->setText(meta.name);
        this->item.name = meta.name;
    }

    // Keep the episodes so the Episodes button can open the season list
    // without refetching the meta, and the backdrop as episode-thumb fallback.
    this->videos = meta.videos;
    this->background = meta.background;
    if (!this->videos.empty() && this->item.type != "movie") this->btnLabel->setText("▶  Episódios");

    // Year · runtime · ★ rating (· N seasons for series)
    std::vector<std::string> parts;
    std::string year = !meta.releaseInfo.empty() ? meta.releaseInfo : this->item.year;
    if (!year.empty()) parts.push_back(year);
    if (!meta.runtime.empty()) parts.push_back(meta.runtime);
    if (!meta.imdbRating.empty()) parts.push_back("★ " + meta.imdbRating);
    if (this->item.type == "series" && !meta.videos.empty()) {
        std::set<int> seasons;
        for (auto& v : meta.videos)
            if (v.season > 0) seasons.insert(v.season);
        if (!seasons.empty())
            parts.push_back(seasons.size() == 1 ? "1 temporada" : fmt::format("{} temporadas", seasons.size()));
    }
    if (!parts.empty()) this->labelMeta->setText(join(parts, " · "));

    if (!meta.genres.empty()) {
        this->labelGenres->setText(join(meta.genres, " · "));
        this->labelGenres->setVisibility(brls::Visibility::VISIBLE);
    }

    // Keep a readable synopsis preview; Ver mais opens the full text.
    this->fullDesc = !meta.description.empty() ? meta.description : "Nenhuma sinopse disponível.";
    const size_t maxInline = 240;
    if (this->fullDesc.size() > maxInline) {
        std::string cut = this->fullDesc.substr(0, maxInline);
        size_t sp = cut.find_last_of(' ');
        if (sp != std::string::npos && sp > maxInline / 2) cut.resize(sp);
        this->labelDesc->setText(cut + "…");
        this->btnMore->setVisibility(brls::Visibility::VISIBLE);
    } else {
        this->labelDesc->setText(this->fullDesc);
        this->btnMore->setVisibility(brls::Visibility::GONE);
    }

    if (!meta.cast.empty()) {
        this->labelCast->setText(join(meta.cast));
        this->labelCastHead->setVisibility(brls::Visibility::VISIBLE);
        this->labelCast->setVisibility(brls::Visibility::VISIBLE);
    }

    if (!meta.director.empty()) {
        this->labelDirector->setText("Direção: " + join(meta.director));
        this->labelDirector->setVisibility(brls::Visibility::VISIBLE);
    }
}

void StremioDetail::onAction() {
    // Series — and anime whose Kitsu meta lists episodes — open the list.
    if (this->item.type == "series" || !this->videos.empty()) {
        // Reuse the episodes we already fetched; fall back to the fetching
        // constructor if the meta hasn't arrived (or failed).
        if (!this->videos.empty())
            brls::Application::pushActivity(
                new brls::Activity(new StremioSeries(this->item.name, this->videos, this->background)),
                brls::TransitionAnimation::NONE);
        else
            brls::Application::pushActivity(
                new brls::Activity(new StremioSeries(this->item)), brls::TransitionAnimation::NONE);
        return;
    }
    this->openStreams();
}

void StremioDetail::openStreams() {
    if (this->fetching) return;  // ignore repeat presses while a request runs
    stremio_playback::interruptResume();
    if (stremio::STREAM_ADDON.empty()) {
        brls::Application::notify("Configure o complemento primeiro (pressione − na tela inicial)");
        return;
    }
    this->fetching = true;
    brls::Application::notify("Buscando vídeos…");

    std::string name = this->item.name;
    ASYNC_RETAIN
    stremio::fetchStreams(this->item.type, this->item.id,
        [ASYNC_TOKEN, name](stremio::StreamList r, std::string usedType) {
            ASYNC_RELEASE
            this->fetching = false;
            if (r.streams.empty()) {
                brls::Application::notify("Nenhum vídeo encontrado");
                return;
            }
            ResumeEntry key{usedType, this->item.id, this->item.name, this->item.poster};
            brls::Application::pushActivity(new brls::Activity(new StreamPicker(name, r.streams, key)));
        },
        [ASYNC_TOKEN](const std::string& e) {
            ASYNC_RELEASE
            this->fetching = false;
            brls::Application::notify("Erro ao buscar vídeos: " + e);
        });
}
