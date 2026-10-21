#include "activity/stremio_addons.hpp"
#include "api/stremio.hpp"
#include "utils/config.hpp"
#include "view/stremio_theme.hpp"
#include "view/stremio_controls.hpp"
using namespace brls::literals;

StremioAddons::StremioAddons(std::function<void()> onClose) {
    setAxis(brls::Axis::COLUMN); setPadding(28,60,28,60);
    setDimensions(brls::Application::contentWidth,brls::Application::contentHeight);
    auto* title=new brls::Label(); title->setText("Complementos de vídeo");
    title->setFontSize(30); title->setHeight(44); title->setShrink(0); title->setTextColor(stremio_theme::TEXT); addView(title);
    auto* intro=new brls::Label();
    intro->setText("Salve seus endereços e escolha qual usar. Apenas o complemento ativo busca vídeos.");
    intro->setFontSize(18); intro->setHeight(32); intro->setShrink(0); intro->setTextColor(stremio_theme::TEXT_DIM); addView(intro);
    addButton=new brls::Button(); addButton->setText("+ Adicionar complemento"); addButton->setShrink(0);
    addButton->setMarginTop(16); addButton->setMarginBottom(18); addButton->setBackgroundColor(nvgRGB(51,101,148));
    addButton->registerClickAction([this](brls::View*) { askUrl(); return true; }); addView(addButton);
    auto* scroll=new brls::ScrollingFrame(); scroll->setGrow(1); scroll->setScrollingIndicatorVisible(true);
    list=new brls::Box(); list->setAxis(brls::Axis::COLUMN); list->setPaddingRight(16);
    scroll->setContentView(list); addView(scroll);
    auto* later=new brls::Button(); later->setText(stremio::STREAM_ADDON.empty() ? "Voltar sem configurar" : "Concluir e voltar"); later->setShrink(0);
    closeButton=later;
    later->setMarginTop(12);
    later->registerClickAction([onClose](brls::View*) {
        brls::Application::popActivity(); if (onClose) onClose(); return true;
    }); addView(later);
    stremio_theme::controls(this, {{"A","Selecionar complemento"},{"B","Voltar"}});
    registerAction("hints/back"_i18n,brls::BUTTON_B,[onClose](brls::View*) {
        brls::Application::popActivity(); if (onClose) onClose(); return true;
    });
    rebuild();
    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN]() { ASYNC_RELEASE brls::Application::giveFocus(addButton); });
}
void StremioAddons::rebuild() {
    closeButton->setText(stremio::STREAM_ADDON.empty() ? "Configurar depois" : "Concluir e voltar");
    list->clearViews(true);
    addButton->setText(stremio::VIDEO_ADDONS.entries.empty() ? "Adicionar outro endereço" : "+ Adicionar complemento");
    if (stremio::VIDEO_ADDONS.entries.empty()) {
        auto* useFenix=new brls::Button(); useFenix->setText("Usar FenixFlix");
        useFenix->setShrink(0); useFenix->setMarginBottom(16); useFenix->setBackgroundColor(nvgRGB(28,79,74));
        useFenix->registerClickAction([this](brls::View*) {
            auto next=stremio::VIDEO_ADDONS;
            if (next.add("FenixFlix","https://fenixflix.fenixhub.online/manifest/json",true)) commit(next);
            return true;
        }); list->addView(useFenix);
        auto* empty=new brls::Label(); empty->setText("Escolha FenixFlix para configurar com um clique ou adicione outro endereço.\nFenixFlix é um serviço de terceiros. Nenhum complemento é ativado sem sua escolha.");
        empty->setWidthPercentage(100); empty->setSingleLine(false); empty->setFontSize(20); empty->setLineHeight(1.35f);
        empty->setTextColor(stremio_theme::TEXT_DIM); list->addView(empty); return;
    }
    if (stremio::STREAM_ADDON.empty()) {
        auto* note=new brls::Label(); note->setText("Nenhum complemento ativo. Selecione um endereço salvo para usar.");
        note->setFontSize(18); note->setHeight(32); note->setShrink(0); note->setTextColor(stremio_theme::TEXT_DIM); list->addView(note);
    }
    for (const auto& entry:stremio::VIDEO_ADDONS.entries) {
        bool active=entry.url==stremio::STREAM_ADDON;
        auto* card=new brls::Box(); card->setAxis(brls::Axis::COLUMN); card->setFocusable(true);
        card->setShrink(0); card->setPadding(15,20,15,20); card->setMarginBottom(12); card->setCornerRadius(12);
        card->setBackgroundColor(active ? nvgRGB(28,79,74) : nvgRGB(28,40,62));
        auto* status=new brls::Box(); status->setAxis(brls::Axis::ROW); status->setShrink(0);
        status->addView(new stremio_theme::RadioDot(active));
        auto* name=new brls::Label(); name->setText((active ? "ATIVO · " : "")+entry.name);
        name->setFontSize(22); name->setHeight(32); name->setShrink(1);
        name->setSingleLine(true); name->setAutoAnimate(false); name->setTextColor(active ? nvgRGB(120,235,200) : stremio_theme::TEXT); name->setGrow(1); status->addView(name); card->addView(status);
        auto* detail=new brls::Label(); detail->setText(addon_profiles::host(entry.url)+" · A para selecionar ou editar");
        detail->setFontSize(16); detail->setHeight(26); detail->setShrink(0); detail->setWidthPercentage(100);
        detail->setTextColor(stremio_theme::TEXT_DIM); card->addView(detail);
        card->registerClickAction([this,entry](brls::View*) { actions(entry); return true; });
        card->addGestureRecognizer(new brls::TapGestureRecognizer(card)); list->addView(card);
    }
}
void StremioAddons::refresh() {
    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN]() { ASYNC_RELEASE
        brls::Application::giveFocus(addButton); rebuild();
    });
}
void StremioAddons::commit(const addon_profiles::State& next) {
    if (!stremio::setVideoAddons(AppConfig::instance().configDir(),next)) {
        brls::Application::notify("Não foi possível salvar no cartão SD"); return;
    }
    brls::Application::notify("Complementos salvos"); refresh();
}
void StremioAddons::actions(addon_profiles::Entry entry) {
    auto* dialog=new brls::Dialog(entry.name+"\n"+addon_profiles::host(entry.url));
    dialog->addButton("Usar este",[this,entry]() { auto next=stremio::VIDEO_ADDONS; if (next.select(entry.url)) commit(next); });
    dialog->addButton("Editar",[this,entry]() {
        auto* edit=new brls::Dialog("Editar "+entry.name);
        edit->addButton("Renomear",[this,entry]() { askName(entry,entry.url); });
        edit->addButton("Alterar endereço",[this,entry]() { askUrl(entry); });
        edit->addButton("Voltar",[]() {}); edit->open();
    });
    dialog->addButton("Excluir",[this,entry]() {
        auto* confirm=new brls::Dialog("Excluir "+entry.name+"? A biblioteca e o histórico serão preservados."+(entry.url==stremio::STREAM_ADDON ? " Escolha outro complemento depois para assistir." : ""));
        confirm->addButton("Excluir",[this,entry]() { auto next=stremio::VIDEO_ADDONS; if (next.remove(entry.url)) commit(next); });
        confirm->addButton("Cancelar",[]() {}); confirm->open();
    }); dialog->open();
}
void StremioAddons::askUrl(addon_profiles::Entry entry) {
    if (entry.url.empty() && stremio::VIDEO_ADDONS.entries.size()>=20) { brls::Application::notify("Você já tem 20 complementos salvos"); return; }
    ASYNC_RETAIN
    brls::Application::getImeManager()->openForText([ASYNC_TOKEN,entry](const std::string& value) {
        ASYNC_RELEASE
        if (addon_profiles::trim(value).empty()) return;
        auto url=stremio::normalizeAddonUrl(value);
        if (url.empty()) { brls::Application::notify("Endereço inválido. Use HTTP ou HTTPS."); return; }
        askName(entry,url);
    },"Endereço do complemento","Cole o endereço do complemento ou seu manifest.json.",2048,entry.url,0);
}
void StremioAddons::askName(addon_profiles::Entry entry,const std::string& url) {
    ASYNC_RETAIN
    brls::Application::getImeManager()->openForText([ASYNC_TOKEN,entry,url](const std::string& value) {
        ASYNC_RELEASE
        if (addon_profiles::trim(value).empty()) return;
        auto next=stremio::VIDEO_ADDONS;
        bool ok=entry.url.empty() ? next.add(value,url,true) : next.edit(entry.url,value,url);
        if (!ok) { brls::Application::notify("Endereço já salvo ou nome inválido"); return; }
        commit(next);
    },"Nome do complemento","Escolha um nome para reconhecer este endereço na lista.",48,entry.name.empty() ? addon_profiles::host(url) : entry.name,0);
}
void StremioAddons::draw(NVGcontext* vg,float x,float y,float w,float h,brls::Style s,brls::FrameContext* c) {
    stremio_theme::drawOceanBackground(vg,x,y,w,h); brls::Box::draw(vg,x,y,w,h,s,c);
}
