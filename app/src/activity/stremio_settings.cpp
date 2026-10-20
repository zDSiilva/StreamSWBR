#include "activity/stremio_settings.hpp"
#include "activity/stremio_rows.hpp"
#include "activity/stream_preferences.hpp"
#include "api/stremio.hpp"
#include "api/portuguese_metadata.hpp"
#include "utils/config.hpp"
#include "view/stremio_theme.hpp"
#include "view/svg_image.hpp"

using namespace brls::literals;
namespace {
brls::Box* setting(brls::Box* parent, const std::string& title, const std::string& detail,
    brls::Label** output) {
    auto* row = new brls::Box();
    row->setAxis(brls::Axis::COLUMN);
    row->setFocusable(true);
    row->setPadding(16, 22, 16, 22);
    row->setShrink(0);
    row->setCornerRadius(12);
    row->setMarginBottom(14);
    row->setBackgroundColor(nvgRGBA(255, 255, 255, 16));
    auto* name = new brls::Label();
    name->setText(title); name->setFontSize(23); name->setTextColor(stremio_theme::TEXT);
    name->setMinHeight(32); name->setMarginBottom(7); name->setShrink(0);
    name->setSingleLine(true); name->setAutoAnimate(false);
    row->addView(name);
    auto* value = new brls::Label();
    value->setText(detail); value->setFontSize(17); value->setTextColor(stremio_theme::TEXT_DIM);
    value->setMinHeight(25); value->setLineHeight(1.35f); value->setShrink(0);
    value->setSingleLine(false); value->setAutoAnimate(false);
    row->addView(value);
    row->addGestureRecognizer(new brls::TapGestureRecognizer(row));
    parent->addView(row);
    if (output) *output = value;
    return row;
}
void saved(bool ok) {
    brls::Application::notify(ok ? "Preferência salva" : "Não foi possível salvar no cartão SD");
}

class TmdbHelp : public brls::Box {
public:
    explicit TmdbHelp(std::function<void()> onConfigure) {
        setAxis(brls::Axis::COLUMN); setPadding(26, 60, 26, 60);
        setDimensions(brls::Application::contentWidth, brls::Application::contentHeight);
        auto* title = new brls::Label(); title->setText("Títulos e sinopses em português");
        title->setFontSize(30); title->setHeight(44); title->setShrink(0);
        title->setTextColor(stremio_theme::TEXT); addView(title);
        auto* intro = new brls::Label();
        intro->setText("Os menus já estão traduzidos. O TMDB fornece os nomes e as sinopses dos filmes e séries. Configure uma vez para manter essas informações em português.");
        intro->setFontSize(20); intro->setLineHeight(1.35f); intro->setMinHeight(70);
        intro->setShrink(0); intro->setMarginBottom(18); intro->setTextColor(stremio_theme::TEXT_DIM); addView(intro);
        auto* body = new brls::Box(); body->setAxis(brls::Axis::ROW); body->setGrow(1); addView(body);
        auto* scroll = new brls::ScrollingFrame(); scroll->setGrow(1); scroll->setScrollingIndicatorVisible(true);
        auto* steps = new brls::Box(); steps->setAxis(brls::Axis::COLUMN); steps->setPaddingRight(24);
        const char* names[] = {"1. Entre no TMDB pelo celular ou computador", "2. Abra Configurações da conta → API",
            "3. Copie o token de leitura", "4. Volte ao aplicativo e salve"};
        const char* details[] = {"Leia o QR code ao lado ou acesse themoviedb.org/settings/api. Entre na sua conta ou crie uma conta no site.",
            "Se ainda não tiver acesso à API, solicite uma chave de desenvolvedor e preencha o formulário do seu projeto no site.",
            "Procure API Read Access Token. Copie o valor inteiro. Use esse token, não a chave curta da API nem a senha da sua conta.",
            "Selecione Inserir token abaixo. Para evitar digitar no Switch, adicione tmdb_token=SEU_TOKEN ao arquivo /switch/streamfin-addon.txt no cartão SD e reinicie o app."};
        for (int i = 0; i < 4; ++i) {
            auto* card = setting(steps, names[i], details[i], nullptr);
            card->setFocusable(true); // Controller focus scrolls each tutorial step into view.
        }
        scroll->setContentView(steps); body->addView(scroll);
        auto* aside = new brls::Box(); aside->setAxis(brls::Axis::COLUMN); aside->setWidth(250); aside->setShrink(0);
        aside->setPadding(12, 10, 12, 10); aside->setCornerRadius(12); aside->setBackgroundColor(nvgRGBA(255,255,255,12));
        auto* qr = new brls::Image(); qr->setDimensions(220,220); qr->setShrink(0);
        qr->setScalingType(brls::ImageScalingType::FIT); qr->setImageFromRes("img/tmdb-setup-qr.png"); aside->addView(qr);
        auto* caption = new brls::Label(); caption->setText("Aponte a câmera do celular\n\nDepois de salvar, reabra a tela inicial. Quando não houver tradução cadastrada, o título pode manter o nome original.");
        caption->setFontSize(18); caption->setLineHeight(1.35f); caption->setMarginTop(12);
        caption->setTextColor(stremio_theme::TEXT_DIM); aside->addView(caption); body->addView(aside);
        auto* footer = new brls::Box(); footer->setAxis(brls::Axis::ROW); footer->setMarginTop(18); footer->setShrink(0);
        auto* enter = new brls::Button(); enter->setText("Inserir token"); enter->setBackgroundColor(nvgRGB(105,64,195));
        enter->registerClickAction([onConfigure](brls::View*) {
            auto configure = onConfigure; brls::Application::popActivity(); configure(); return true;
        }); footer->addView(enter);
        auto* back = new brls::Button(); back->setText("Voltar"); back->setMarginLeft(16);
        back->setBackgroundColor(nvgRGB(40,92,154));
        back->registerClickAction([](brls::View*) { brls::Application::popActivity(); return true; }); footer->addView(back);
        addView(footer);
        registerAction("hints/back"_i18n, brls::BUTTON_B, [](brls::View*) { brls::Application::popActivity(); return true; });
        brls::sync([scroll]() { brls::Application::giveFocus(scroll); });
    }
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override {
        stremio_theme::drawOceanBackground(vg, x, y, width, height); brls::Box::draw(vg,x,y,width,height,style,ctx);
    }
};
}
StremioSettings::StremioSettings(std::function<void()> onRowsChanged) {
    setAxis(brls::Axis::COLUMN);
    setDimensions(brls::Application::contentWidth, brls::Application::contentHeight);
    setPadding(24, 60, 24, 60);
    auto* heading = new brls::Label();
    heading->setText("Configurações"); heading->setFontSize(32); heading->setMarginBottom(18);
    heading->setHeight(44); heading->setShrink(0);
    heading->setTextColor(stremio_theme::TEXT); addView(heading);
    auto* navigation = new brls::Label(); navigation->setText("↑ ↓ navegar · A selecionar · B voltar");
    navigation->setFontSize(17); navigation->setHeight(28); navigation->setShrink(0);
    navigation->setMarginBottom(14); navigation->setTextColor(stremio_theme::TEXT_DIM); addView(navigation);
    auto* scroll = new brls::ScrollingFrame(); scroll->setGrow(1); scroll->setScrollingIndicatorVisible(false);
    auto* content = new brls::Box(); content->setAxis(brls::Axis::COLUMN); scroll->setContentView(content);
    addView(scroll);
    auto* categories = setting(content, "Organizar categorias", "Escolha as fileiras da tela inicial e a ordem das capas", nullptr);
    categories->registerClickAction([this](brls::View*) {
        brls::Application::pushActivity(new brls::Activity(new StremioRows([this]() { rowsChanged = true; }))); return true;
    });
    const char* names[] = {"Informações em português · TMDB", "Servidor preferido", "Complemento de vídeo", "Complemento de legendas"};
    auto prefs = stream_preferences::get();
    const std::string values[] = {prefs.tmdbToken.empty() ? "Ver o passo a passo para ativar títulos e sinopses em português" : "Configuração salva · ver tutorial ou atualizar token",
        prefs.preferredServer.empty() ? "Nenhum · manter a ordem do complemento" : prefs.preferredServer,
        stremio::STREAM_ADDON.empty() ? "Configurar endereço" : "Endereço configurado · A para editar",
        stremio::SUBTITLES_ADDON.empty() ? "Opcional · configurar endereço" : "Endereço configurado · A para editar"};
    for (int field = 0; field < 4; ++field) {
        brls::Label* label = nullptr;
        auto* row = setting(content, names[field], values[field], &label);
        if (field == 0) row->setBackgroundColor(nvgRGB(33, 60, 99));
        row->registerClickAction([this, field, label](brls::View*) {
            if (field == 0) tmdbHelp(label); else edit(field, label); return true;
        });
    }
    brls::Label* audioLabel = nullptr;
    auto* audio = setting(content, "Áudio preferido", prefs.audio.empty() ? "Automático" : prefs.audio == "eng,en" ? "Inglês" : "Português", &audioLabel);
    audio->registerClickAction([audioLabel](brls::View*) {
        auto next = stream_preferences::get();
        next.audio = next.audio.empty() ? "por,pt,pt-BR,eng,en" : next.audio == "eng,en" ? "" : "eng,en";
        bool ok = stream_preferences::save(next); saved(ok);
        if (ok) audioLabel->setText(next.audio.empty() ? "Automático" : next.audio == "eng,en" ? "Inglês" : "Português");
        return true;
    });
    brls::Label* subtitlesLabel = nullptr;
    auto* subtitles = setting(content, "Legendas preferidas", prefs.subtitles == "off" ? "Desativadas ao iniciar" : "Português, quando disponível", &subtitlesLabel);
    subtitles->registerClickAction([subtitlesLabel](brls::View*) {
        auto next = stream_preferences::get(); next.subtitles = next.subtitles == "off" ? "por,pt,pt-BR" : "off";
        bool ok = stream_preferences::save(next); saved(ok);
        if (ok) subtitlesLabel->setText(next.subtitles == "off" ? "Desativadas ao iniciar" : "Português, quando disponível");
        return true;
    });
    auto* cache = setting(content, "Atualizar informações em português", "Limpar o cache de traduções; biblioteca e histórico são preservados", nullptr);
    cache->registerClickAction([](brls::View*) {
        brls::async([]() { portuguese_metadata::clearCache(); brls::sync([]() {
            brls::Application::notify("Cache de traduções limpo. Reabra os títulos para atualizar.");
        }); }); return true;
    });
    auto* about = new brls::Label();
    auto* attributionLogo = new SVGImage();
    attributionLogo->setDimensions(112, 48);
    attributionLogo->setMarginTop(18);
    attributionLogo->setImageFromSVGRes("icon/tmdb-approved.svg");
    content->addView(attributionLogo);
    about->setText("StreamFinEX · Português · Revisão visual 2\nDados: TMDB · themoviedb.org\nThis product uses the TMDB API but is not endorsed or certified by TMDB.");
    about->setFontSize(15); about->setTextColor(stremio_theme::TEXT_DIM); about->setIsWrapping(true);
    about->setMarginTop(16); content->addView(about);
    registerAction("hints/back"_i18n, brls::BUTTON_B, [this, onRowsChanged](brls::View*) {
        const bool changed = rowsChanged;
        const auto apply = onRowsChanged;
        brls::Application::popActivity();
        if (changed && apply) apply();
        return true;
    });
    brls::sync([categories]() { brls::Application::giveFocus(categories); });
}
void StremioSettings::tmdbHelp(brls::Label* label) {
    brls::Application::pushActivity(new brls::Activity(new TmdbHelp([this, label]() { edit(0, label); })));
}
void StremioSettings::edit(int field, brls::Label* label) {
    const char* titles[] = {"Token de leitura da API do TMDB", "Servidor preferido (vazio para nenhum)", "Endereço do complemento de vídeo", "Endereço do complemento de legendas"};
    auto prefs = stream_preferences::get();
    std::string initial = field == 0 ? "" : field == 1 ? prefs.preferredServer : field == 2 ? stremio::STREAM_ADDON : stremio::SUBTITLES_ADDON;
    ASYNC_RETAIN
    brls::Application::getImeManager()->openForText([ASYNC_TOKEN, field, label](const std::string& value) {
        ASYNC_RELEASE
        auto next = stream_preferences::get();
        if (field == 0) {
            auto tokenValue = stremio::trimJunk(value);
            if (tokenValue.empty()) return; // Cancelling never clears an existing token.
            if (tokenValue.find_first_of("\r\n ") != std::string::npos) {
                brls::Application::notify("Cole somente o token de leitura, sem o prefixo Bearer"); return;
            }
            next.tmdbToken = tokenValue;
        }
        if (field == 1) next.preferredServer = stremio::trimJunk(value);
        if (field < 2) {
            bool ok = stream_preferences::save(next); saved(ok);
            if (ok) label->setText(field == 0 ? "Token configurado · reabra a tela inicial para carregar os títulos" :
                next.preferredServer.empty() ? "Nenhum · manter a ordem do complemento" : next.preferredServer);
        } else {
            const auto url = stremio::normalizeAddonUrl(value);
            if (url.empty() && (field == 2 || !value.empty())) {
                brls::Application::notify("Endereço inválido. Use um endereço HTTP ou HTTPS."); return;
            }
            if (field == 2) stremio::STREAM_ADDON = url; else stremio::SUBTITLES_ADDON = url;
            stremio::saveConfig(AppConfig::instance().configDir());
            label->setText(url.empty() ? "Complemento desativado" : "Endereço configurado · A para editar");
            brls::Application::notify("Complemento salvo");
        }
    }, titles[field], field == 0 ? "Configure na sua conta em themoviedb.org/settings/api. O token fica apenas no cartão SD." : "",
        2048, initial, 0);
}
void StremioSettings::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    stremio_theme::drawOceanBackground(vg, x, y, width, height);
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}
