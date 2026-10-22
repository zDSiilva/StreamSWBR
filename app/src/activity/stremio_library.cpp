#include "view/stremio_controls.hpp"
/*
    Stremio library screen -- implementation.
*/
#include "activity/stremio_library.hpp"
#include "activity/stremio_detail.hpp"
#include "activity/stremio_favourites.hpp"
#include "activity/stream_options.hpp"
#include "view/recycling_grid.hpp"
#include "view/video_card.hpp"
#include "view/svg_image.hpp"
#include "view/stremio_theme.hpp"
#include "utils/image.hpp"

#include <algorithm>

using namespace brls::literals;

namespace {

const char* SORT_NAMES[] = {"Recentes", "Nome", "Ano", "Nota"};

std::string lower(std::string s) {
    return stream_options::lower(std::move(s));
}

// Grid source: same cell styling as the home rows; A opens the detail screen,
// X removes from the library (badge always shown — everything here is saved).
class LibraryGridSource : public RecyclingGridDataSource {
public:
    explicit LibraryGridSource(std::vector<stremio::Meta> r) : list(std::move(r)) {}

    size_t getItemCount() override { return this->list.size(); }

    RecyclingGridItem* cellForRow(RecyclingView* recycler, size_t index) override {
        FavCardCell* cell = dynamic_cast<FavCardCell*>(recycler->dequeueReusableCell("Cell"));
        auto item = this->list.at(index);

        cell->bindTitle(item.id, item.type, item.name);
        if (item.year.empty()) {
            cell->labelExt->setVisibility(brls::Visibility::GONE);
        } else {
            cell->labelExt->setText(item.year);
            cell->labelExt->setVisibility(brls::Visibility::VISIBLE);
        }
        if (item.imdbRating.empty() || !stremio::POSTER_TEMPLATE.empty()) {
            cell->labelRating->setVisibility(brls::Visibility::INVISIBLE);
        } else {
            cell->labelRating->setText("★ " + item.imdbRating);
            cell->labelRating->setVisibility(brls::Visibility::VISIBLE);
        }
        cell->badgeTopRight->setVisibility(brls::Visibility::GONE);
        cell->rectProgress->getParent()->setVisibility(brls::Visibility::GONE);
        cell->badgeFavorite->setVisibility(brls::Visibility::VISIBLE);
        cell->onToggleFav = [item]() {
            Favourites::instance().toggle(item);  // the changed() event rebuilds the grid
        };

        std::string poster = stremio::posterUrl(item.id, item.poster);
        if (!poster.empty()) Image::with(cell->picture, poster);
        return cell;
    }

    void onItemSelected(brls::Box* recycler, size_t index) override {
        brls::Application::pushActivity(
            new brls::Activity(new StremioDetail(this->list.at(index))), brls::TransitionAnimation::NONE);
    }

    void clearData() override { this->list.clear(); }

private:
    std::vector<stremio::Meta> list;
};

// Small focusable pill button for the top bar.
brls::Box* makePill(brls::Label** lblOut, const std::string& text) {
    auto* pill = new brls::Box();
    pill->setFocusable(true);
    pill->setAxis(brls::Axis::ROW);
    pill->setPadding(8, 18, 8, 18);
    pill->setCornerRadius(6);
    pill->setMarginLeft(12);
    pill->setBackgroundColor(nvgRGBA(255, 255, 255, 22));
    pill->setHideHighlightBackground(true);
    auto* lbl = new brls::Label();
    lbl->setText(text);
    lbl->setFontSize(20);
    lbl->setTextColor(stremio_theme::TEXT);
    pill->addView(lbl);
    *lblOut = lbl;
    return pill;
}

}  // namespace

StremioLibrary::StremioLibrary() {
    this->setAxis(brls::Axis::COLUMN);
    this->setDimensions(brls::Application::contentWidth, brls::Application::contentHeight);
    this->setPadding(20, 40, 20, 40);

    // ---- Top bar: headline + search + sort --------------------------------
    auto* top = new brls::Box();
    top->setAxis(brls::Axis::ROW);
    top->setAlignItems(brls::AlignItems::CENTER);
    top->setMarginBottom(14);

    this->headline = new brls::Label();
    this->headline->setFontSize(27);
    this->headline->setMarginLeft(6);
    this->headline->setTextColor(stremio_theme::TEXT);
    this->headline->setGrow(1.0f);
    top->addView(this->headline);

    this->btnSearch = makePill(&this->lblSearch, "🔍 Buscar");
    this->btnSort = makePill(&this->lblSort, "Ordenar: Recentes");
    top->addView(this->btnSearch);
    top->addView(this->btnSort);
    this->addView(top);

    auto doSearch = [this]() {
        brls::Application::getImeManager()->openForText(
            [this](const std::string& text) {
                this->filter = lower(text);
                this->lblSearch->setText(text.empty() ? "🔍 Buscar" : "🔍 " + text);
                this->rebuild();
            },
            "Buscar nos favoritos", "Deixe vazio para limpar o filtro", 64, this->filter, 0);
    };
    this->btnSearch->registerClickAction([doSearch](brls::View*) {
        doSearch();
        return true;
    });
    this->btnSearch->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnSearch));
    this->registerAction("Buscar", brls::BUTTON_Y, [doSearch](brls::View*) {
        doSearch();
        return true;
    });

    this->btnSort->registerClickAction([this](brls::View*) {
        this->sortMode = (this->sortMode + 1) % 4;
        this->lblSort->setText(std::string("Ordenar: ") + SORT_NAMES[this->sortMode]);
        this->rebuild();
        return true;
    });
    this->btnSort->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnSort));

    // ---- Grid --------------------------------------------------------------
    this->recycler = new RecyclingGrid();
    this->recycler->setGrow(1.0f);
    this->recycler->setScrollingIndicatorVisible(false);
    this->recycler->spanCount = 6;
    this->recycler->estimatedRowHeight = 334;
    this->recycler->registerCell("Cell", FavCardCell::create);
    this->addView(this->recycler);
    stremio_theme::controls(this, {{"A","Selecionar"},{"B","Voltar"}});

    this->registerAction("hints/back"_i18n, brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    // X removals rebuild the grid; deferred a frame so the press finishes
    // before its cell is recycled. Park focus off the grid first so the
    // highlight can't be left on a recycled cell.
    this->changedSub = Favourites::instance().changed()->subscribe([this]() {
        brls::sync([this]() {
            brls::Application::giveFocus(this->btnSort);
            this->rebuild();
            brls::sync([this]() {
                if (!Favourites::instance().all().empty()) brls::Application::giveFocus(this->recycler);
            });
        });
    });

    this->rebuild();
    brls::sync([this]() { brls::Application::giveFocus(this->recycler); });
}

StremioLibrary::~StremioLibrary() { Favourites::instance().changed()->unsubscribe(this->changedSub); }

void StremioLibrary::draw(
    NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    stremio_theme::drawOceanBackground(vg, x, y, width, height);
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

void StremioLibrary::rebuild() {
    auto& favs = Favourites::instance().all();
    std::vector<stremio::Meta> list(favs.rbegin(), favs.rend());  // newest first

    if (!this->filter.empty()) {
        std::vector<stremio::Meta> kept;
        for (auto& m : list)
            if (lower(portuguese_metadata::cachedTitle(m.id, m.type, m.name)).find(this->filter) != std::string::npos) kept.push_back(m);
        list = std::move(kept);
    }

    switch (this->sortMode) {
    case 1:  // name A→Z
        std::sort(list.begin(), list.end(),
            [](const stremio::Meta& a, const stremio::Meta& b) { return lower(portuguese_metadata::cachedTitle(a.id, a.type, a.name)) < lower(portuguese_metadata::cachedTitle(b.id, b.type, b.name)); });
        break;
    case 2:  // year, newest first
        std::sort(list.begin(), list.end(),
            [](const stremio::Meta& a, const stremio::Meta& b) { return a.year > b.year; });
        break;
    case 3:  // rating, highest first
        std::sort(list.begin(), list.end(), [](const stremio::Meta& a, const stremio::Meta& b) {
            double ra = atof(a.imdbRating.c_str()), rb = atof(b.imdbRating.c_str());
            return ra > rb;
        });
        break;
    default:
        break;  // recent = insertion order, already reversed
    }

    this->headline->setText(fmt::format("Favoritos · {} título{}", list.size(), list.size() == 1 ? "" : "s"));
    if (list.empty()) {
        this->recycler->setEmpty(this->filter.empty() ? "Favoritos vazios — pressione X em um pôster para adicionar"
                                                      : "Nenhum título corresponde à busca");
    } else {
        this->recycler->setDataSource(new LibraryGridSource(std::move(list)));
    }
}
