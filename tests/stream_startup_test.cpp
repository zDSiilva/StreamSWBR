#include "api/stremio.hpp"
#include <cassert>
#include <future>
#include <iostream>
using namespace std::chrono_literals;
namespace portuguese_metadata {
inline bool allowDetail=false;
void localizeDetail(nlohmann::json&) { assert(allowDetail && "background metadata queue must remain blocked"); }
void localizeCatalog(nlohmann::json&) { assert(false && "metadata queue must remain blocked"); }
}
static std::string playable() { return R"({"streams":[{"name":"Malfoy","url":"https://video.example/test.mp4"}]})"; }
int main() {
    stremio::STREAM_ADDON="https://addon.example";
    // Deliberately never run the UI/metadata queue: cold-start playback must bypass it.
    stremio::getJSON<stremio::MetaList>([](auto){assert(false);},[](auto){assert(false);},"https://catalog.example");
    assert(brls::metadataTasks.size()==1);
    std::atomic_int attempts{0};
    HTTP::transport=[&](const auto&,auto) { if (++attempts==1) throw std::runtime_error("Could not resolve hostname"); return playable(); };
    std::promise<bool> cold; auto coldResult=cold.get_future();
    stremio::fetchStreams("series","tt123:1:3",[&](auto result,auto type) { cold.set_value(result.streams.size()==1 && type=="series"); },[&](auto){cold.set_value(false);});
    assert(coldResult.wait_for(2s)==std::future_status::ready && coldResult.get());
    assert(attempts==2 && brls::metadataTasks.size()==1);
    HTTP::transport=[](const auto&,auto) { return R"({"meta":{"id":"tt123","videos":[{"id":"tt123:1:4","season":1,"episode":4}]}})"; };
    std::promise<bool> episodeMeta; auto episodeMetaResult=episodeMeta.get_future();
    stremio::playbackMeta<stremio::MetaResult>([&](auto result) { episodeMeta.set_value(result.meta.videos.size()==1); },
        [&](auto){episodeMeta.set_value(false);},"https://catalog.example/meta/series/tt123.json");
    assert(episodeMetaResult.wait_for(2s)==std::future_status::ready && episodeMetaResult.get());

    // A cancelled request must not start playback or prevent selecting another item.
    std::promise<bool> entered; auto enteredResult=entered.get_future();
    std::atomic_bool staleCallback{false};
    HTTP::transport=[&](const auto& url,auto cancel) {
        if (url.find("blocked")!=std::string::npos) {
            entered.set_value(true);
            while (!cancel->load()) std::this_thread::sleep_for(1ms);
            throw std::runtime_error("cancelled");
        }
        return playable();
    };
    auto cancelled=stremio::fetchStreams("movie","blocked",[&](auto,auto){staleCallback=true;},[&](auto){staleCallback=true;});
    assert(enteredResult.wait_for(2s)==std::future_status::ready); cancelled->store(true);
    std::promise<bool> next; auto nextResult=next.get_future();
    stremio::fetchStreams("movie","tt456",[&](auto result,auto){next.set_value(!result.streams.empty());},[&](auto){next.set_value(false);});
    assert(nextResult.wait_for(2s)==std::future_status::ready && nextResult.get());
    assert(!staleCallback);

    // Prefetch is isolated too: a slow upcoming episode cannot block an explicit play request.
    std::promise<bool> prefetchEntered; auto prefetchStarted=prefetchEntered.get_future();
    HTTP::transport=[&](const auto& url,auto cancel) {
        if (url.find("prefetch")!=std::string::npos) {
            prefetchEntered.set_value(true);
            while (!cancel->load()) std::this_thread::sleep_for(1ms);
            throw std::runtime_error("cancelled");
        }
        return playable();
    };
    auto prefetch=stremio::fetchStreams("series","prefetch",[](auto,auto){assert(false);},[](auto){assert(false);},true);
    assert(prefetchStarted.wait_for(2s)==std::future_status::ready);
    std::promise<bool> foreground; auto foregroundResult=foreground.get_future();
    stremio::fetchStreams("movie","tt789",[&](auto,auto){foreground.set_value(true);},[&](auto){foreground.set_value(false);});
    assert(foregroundResult.wait_for(2s)==std::future_status::ready && foregroundResult.get()); prefetch->store(true);
    std::promise<bool> drained; auto drainedResult=drained.get_future();
    stream_worker::prefetch().submit([&](auto){drained.set_value(true);});
    assert(drainedResult.wait_for(2s)==std::future_status::ready);
    // User-selected details run even while home catalog work is still queued.
    portuguese_metadata::allowDetail=true;
    HTTP::transport=[](const auto&,auto) { return R"({"meta":{"id":"tt123","name":"Detalhes"}})"; };
    std::promise<bool> detail; auto detailResult=detail.get_future();
    stremio::getJSON<stremio::MetaResult>([&](auto r){detail.set_value(r.meta.name=="Detalhes");},
        [&](auto){detail.set_value(false);},"https://catalog.example/meta/series/tt123.json",true);
    assert(detailResult.wait_for(2s)==std::future_status::ready && detailResult.get());
    assert(brls::metadataTasks.size()==1 && loading_activity::busy());
    brls::metadataTasks.clear();
    std::promise<bool> detailsDrained; auto detailsDone=detailsDrained.get_future();
    stream_worker::details().submit([&](auto){detailsDrained.set_value(true);});
    assert(detailsDone.wait_for(2s)==std::future_status::ready);
    assert(!loading_activity::busy());
    { auto activity=loading_activity::track(); auto copied=activity; activity.reset(); assert(loading_activity::busy()); }
    assert(!loading_activity::busy());
    stream_worker::Queue navigationQueue;
    std::promise<bool> navigationStarted; auto navigationRunning=navigationStarted.get_future();
    navigationQueue.submit([&](auto cancel){ navigationStarted.set_value(true); while (!cancel->load()) std::this_thread::sleep_for(1ms); });
    assert(navigationRunning.wait_for(2s)==std::future_status::ready);
    std::atomic_bool staleNavigation{false};
    navigationQueue.submit([&](auto){staleNavigation=true;});
    navigationQueue.cancelPending();
    std::promise<bool> newNavigation; auto newNavigationDone=newNavigation.get_future();
    navigationQueue.submit([&](auto){newNavigation.set_value(true);});
    assert(newNavigationDone.wait_for(2s)==std::future_status::ready && !staleNavigation);
    // Explicit origins must not mutate the global add-on or cross episode IDs.
    auto configured=stremio::STREAM_ADDON;
    HTTP::transport=[](const std::string& url,auto) {
        assert(url=="https://froststream.cloutteam.com/stream/series/tt0898266:1:3.json");
        return R"({"streams":[{"name":"Neon","url":"https://video.example/episode.mp4"}]})";
    };
    std::promise<bool> explicitOrigin; auto explicitDone=explicitOrigin.get_future();
    stremio::fetchStreams("series","tt0898266:1:3",[&](auto result,auto type) {
        explicitOrigin.set_value(type=="series" && result.streams.size()==1 &&
            result.streams[0].sourceAddon=="https://froststream.cloutteam.com");
    },[&](auto){explicitOrigin.set_value(false);},false,"https://froststream.cloutteam.com");
    assert(explicitDone.wait_for(2s)==std::future_status::ready && explicitDone.get());
    assert(stremio::STREAM_ADDON==configured);
    // A slow add-on must not hold up another group's results.
    std::promise<bool> slowStarted,releaseSlow,fastDone,slowDone;
    auto slowRunning=slowStarted.get_future(); auto release=releaseSlow.get_future().share();
    auto fastResult=fastDone.get_future(); auto slowResult=slowDone.get_future();
    HTTP::transport=[&](const std::string& url,auto cancel) {
        if (url.find("slow.example")!=std::string::npos) {
            slowStarted.set_value(true); release.wait();
            throw std::runtime_error("offline");
        }
        assert(url=="https://fast.example/stream/series/tt0898266:1:3.json");
        return std::string(R"({"streams":[{"name":"Neon","url":"https://video.example/play"}]})");
    };
    auto slow=stremio::fetchStreams("series","tt0898266:1:3",[&](auto,auto){slowDone.set_value(false);},
        [&](auto){slowDone.set_value(true);},false,"https://slow.example",&stream_worker::search(0));
    assert(slowRunning.wait_for(2s)==std::future_status::ready);
    stremio::fetchStreams("series","tt0898266:1:3",[&](auto result,auto) {
        fastDone.set_value(result.streams.size()==1 && result.streams[0].sourceAddon=="https://fast.example");
    },[&](auto){fastDone.set_value(false);},false,"https://fast.example",&stream_worker::search(1));
    assert(fastResult.wait_for(2s)==std::future_status::ready && fastResult.get());
    slow->store(true); releaseSlow.set_value(true);
    std::promise<bool> searchDrained; auto searchDone=searchDrained.get_future();
    stream_worker::search(0).submit([&](auto){searchDrained.set_value(true);});
    assert(searchDone.wait_for(2s)==std::future_status::ready);
    assert(slowResult.wait_for(10ms)==std::future_status::timeout);
    assert(stremio::STREAM_ADDON==configured);
    std::cout<<"OK: cold startup bypasses metadata, DNS retry, cancellation and prefetch isolation\n";
}
