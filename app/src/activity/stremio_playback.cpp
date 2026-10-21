#include "activity/stremio_playback.hpp"
#include "activity/stremio_streams.hpp"
#include "activity/stream_options.hpp"
#include "activity/stream_preferences.hpp"
#include "activity/playback_policy.hpp"
#include "tab/remote_view.hpp"
#include "view/video_view.hpp"
#include "view/mpv_core.hpp"
#include <cmath>
#include <optional>

namespace stremio_playback {
namespace {
struct Session {
    ResumeEntry current;
    std::optional<ResumeEntry> next;
    std::optional<stremio::Stream> ready;
    std::vector<stremio::Stream> alternatives;
    std::function<void(const std::string&,const std::string&)> replace;
    bool active=false,metaPending=false,sourcePending=false,waiting=false,resolved=false;
};
Session session;
unsigned generation=0;
bool resuming=false;
stream_worker::Cancel resumeCancel;
std::string raw(const stremio::Stream& s) { return s.name+"\n"+s.title+"\n"+s.description; }
void identify(ResumeEntry& key,const stremio::Stream& stream) {
    auto text=raw(stream);
    key.sourceProvider=stream_options::server(text,"");
    key.sourceHost=addon_profiles::host(stream.url);
    key.sourceQuality=stream_options::quality(text); key.sourceAudio=stream_options::audio(text);
    key.sourceAddon=stremio::STREAM_ADDON;
}
int choose(const ResumeEntry& key,const std::vector<stremio::Stream>& streams) {
    if (key.sourceAddon!=stremio::STREAM_ADDON) return -1;
    int best=-1,bestScore=-1;
    for (size_t i=0;i<streams.size();++i) {
        if (streams[i].url.empty()) continue;
        ResumeEntry candidate; identify(candidate,streams[i]);
        int score=playback_policy::score(key.sourceProvider,key.sourceHost,key.sourceQuality,key.sourceAudio,
            candidate.sourceProvider,candidate.sourceHost,candidate.sourceQuality,candidate.sourceAudio);
        if (score>bestScore) { bestScore=score; best=int(i); }
    }
    return best;
}
void begin(ResumeEntry key,const stremio::Stream& stream,bool replace);
void advance() {
    if (!session.active || !session.waiting || !session.next || !session.ready) return;
    auto key=*session.next; auto stream=*session.ready;
    begin(key,stream,true);
}
void unavailable(ResumeEntry key,std::vector<stremio::Stream> streams) {
    session.waiting=false;
    auto ticket=generation;
    brls::sync([key,streams,ticket]() {
        if (ticket!=generation || !session.active) return;
        VideoView::close(true);
        if (!streams.empty()) {
            brls::Application::notify("A fonte anterior não está disponível. Escolha outra para continuar.");
            brls::Application::pushActivity(new brls::Activity(new StreamPicker(key.name,streams,key)));
        } else brls::Application::notify("O próximo episódio não tem vídeo disponível neste complemento.");
    });
}
void resolveNext() {
    if (!session.next || session.sourcePending || session.ready) return;
    if (session.resolved) { if (session.waiting) unavailable(*session.next,session.alternatives); return; }
    session.sourcePending=true; auto ticket=generation;
    auto key=*session.next;
    stremio::fetchStreams(key.streamType,key.streamId,
        [ticket,key](stremio::StreamList result,std::string type) mutable {
            if (ticket!=generation || !session.active) return;
            session.sourcePending=false; key.streamType=type;
            session.resolved=true; session.alternatives=result.streams;
            int selected=choose(session.current,result.streams);
            if (selected>=0) {
                session.next=key; session.ready=result.streams[selected];
                if (session.waiting) advance();
            } else if (session.waiting) unavailable(key,result.streams);
        },[ticket](const std::string& error) {
            if (ticket!=generation || !session.active) return;
            session.sourcePending=false;
            session.resolved=true;
            if (session.waiting) { session.waiting=false; brls::Application::notify(error); VideoView::close(true); }
        },true);
}
void discoverNext(unsigned ticket) {
    auto current=session.current;
    if (current.streamType!="series" && current.streamType!="anime") return;
    auto colon=current.streamId.find(':');
    if (colon==std::string::npos) return;
    bool kitsu=current.streamId.rfind("kitsu:",0)==0;
    auto end=kitsu ? current.streamId.find(':',colon+1) : colon;
    auto id=current.streamId.substr(0,end);
    session.metaPending=true;
    stremio::getJSON<stremio::MetaResult>([ticket,current](stremio::MetaResult result) {
        if (ticket!=generation || !session.active) return;
        session.metaPending=false;
        auto videos=result.meta.videos;
        std::stable_sort(videos.begin(),videos.end(),[](const stremio::Video& a,const stremio::Video& b) {
            return a.season==b.season ? a.episode<b.episode : a.season<b.season;
        });
        for (size_t i=0;i+1<videos.size();++i) if (videos[i].id==current.streamId) {
            auto v=videos[i+1]; if (v.season<=0 || v.episode<=0) break;
            auto series=current.name.substr(0,current.name.find(" · "));
            ResumeEntry key{current.streamType,v.id,series+" · S"+std::to_string(v.season)+"E"+std::to_string(v.episode)+" · "+v.name,v.thumbnail};
            session.next=key; break;
        }
        if (session.waiting) {
            if (session.next) resolveNext();
            else { session.waiting=false; VideoView::close(true); }
        }
    },[ticket](const std::string&) {
        if (ticket!=generation || !session.active) return;
        session.metaPending=false;
        if (session.waiting) { session.waiting=false; VideoView::close(true); }
    },(kitsu ? stremio::KITSU+"/meta/anime/" : stremio::CINEMETA+"/meta/series/")+id+".json");
}
void begin(ResumeEntry key,const stremio::Stream& stream,bool replace) {
    auto player=session.replace;
    ++generation; session=Session{}; session.active=true; session.replace=player;
    identify(key,stream); session.current=key;
    ResumeTracker::instance().setCurrent(key);
    prepareSubtitles(key.streamType,key.streamId);
    auto prefs=stream_preferences::get(); auto& mpv=MPVCore::instance();
    mpv.command("set","alang",prefs.audio.c_str());
    mpv.command("set","slang",prefs.subtitles=="off" ? "" : prefs.subtitles.c_str());
    mpv.command("set","sid",prefs.subtitles=="off" ? "no" : "auto");
    if (replace && player) player(stream.url,key.name);
    else RemoteView::play(stream.url,key.name,true);
    discoverNext(generation);
}
}
void play(ResumeEntry key,const stremio::Stream& stream) { resuming=false; begin(key,stream,false); }
void interruptResume() {
    if (!resuming) return;
    if (resumeCancel) resumeCancel->store(true);
    resuming=false; ++generation;
}
void resume(const ResumeEntry& key) {
    if (resumeCancel) resumeCancel->store(true);
    if (stremio::STREAM_ADDON.empty()) { brls::Application::notify("Selecione um complemento em Configurações."); return; }
    resuming=true; auto ticket=++generation;
    brls::Application::notify("Retomando reprodução…");
    resumeCancel=stremio::fetchStreams(key.streamType,key.streamId,[ticket,key](stremio::StreamList result,std::string type) mutable {
        if (ticket!=generation) return;
        resuming=false;
        auto entry=key; entry.streamType=type;
        int selected=choose(key,result.streams);
        if (selected>=0) play(entry,result.streams[selected]);
        else if (!result.streams.empty()) {
            brls::Application::notify(key.sourceAddon.empty() ? "Escolha uma fonte uma vez para salvar sua preferência." : "A fonte anterior não está disponível. Escolha outra.");
            brls::Application::pushActivity(new brls::Activity(new StreamPicker(key.name,result.streams,entry)));
        } else brls::Application::notify("Nenhum vídeo disponível para retomar.");
    },[ticket](const std::string& error) { if (ticket==generation) { resuming=false; brls::Application::notify(error); } });
    brls::delay(16000,[ticket]() {
        if (ticket!=generation || !resuming) return;
        if (resumeCancel) resumeCancel->store(true);
        ++generation; resuming=false;
        brls::Application::notify("A busca demorou para responder. Você pode tentar novamente ou escolher outro título.");
    });
}
void attach(std::function<void(const std::string&,const std::string&)> replace) { session.replace=replace; }
void cancel() { if (resumeCancel) resumeCancel->store(true); ++generation; session=Session{}; resuming=false; }
void failed() {
    auto key=session.current;
    session.active=false; session.waiting=false;
    auto* dialog=new brls::Dialog("Esta fonte não pôde reproduzir "+key.name+". Você pode escolher outra sem perder o progresso salvo.");
    dialog->addButton("Escolher outra fonte",[key]() {
        VideoView::close(true);
        auto ticket=++generation;
        stremio::fetchStreams(key.streamType,key.streamId,[ticket,key](stremio::StreamList result,std::string type) mutable {
            if (ticket!=generation) return;
            auto entry=key; entry.streamType=type;
            if (!result.streams.empty())
                brls::Application::pushActivity(new brls::Activity(new StreamPicker(entry.name,result.streams,entry)));
            else brls::Application::notify("Nenhum vídeo disponível neste momento.");
        },[ticket](const std::string& error) { if (ticket==generation) brls::Application::notify(error); });
    });
    dialog->addButton("Voltar",[]() { VideoView::close(true); }); dialog->open();
}
bool ended() {
    if (!session.active || (!session.next && !session.metaPending)) return false;
    session.waiting=true;
    if (session.ready) { auto ticket=generation; brls::sync([ticket]() { if (ticket==generation) advance(); }); }
    else resolveNext();
    return true;
}
std::string notice(double position,double duration) {
    if (!session.active) return "";
    if (session.waiting) return "Próximo episódio\nPreparando…";
    if (!session.next || !playback_policy::countdown(position,duration)) return "";
    resolveNext();
    return "Próximo episódio\nComeça em "+std::to_string(int(std::ceil(duration-position)))+" s";
}
}
