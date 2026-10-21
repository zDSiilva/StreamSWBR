#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
namespace stream_worker {
using Cancel=std::shared_ptr<std::atomic_bool>;
// Dedicated playback queue, independent of Borealis' single metadata worker.
class Queue {
    std::mutex mutex;
    std::condition_variable wake;
    bool stopping=false;
    Cancel active;
    struct Task { std::function<void(Cancel)> run; Cancel cancel; };
    std::deque<Task> jobs;
    std::thread worker;
public:
    Queue() : worker([this]() {
        for (;;) {
            std::function<void(Cancel)> task; Cancel token;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock,[this]() { return stopping || !jobs.empty(); });
                if (stopping) return;
                task=std::move(jobs.front().run); token=jobs.front().cancel; jobs.pop_front(); active=token;
            }
            if (!token->load()) { try { task(token); } catch (...) {} }
            { std::lock_guard<std::mutex> lock(mutex); if (active==token) active.reset(); }
        }
    }) {}
    ~Queue() { stop(); }
    void stop() {
        { std::lock_guard<std::mutex> lock(mutex); stopping=true;
          if (active) active->store(true); for (auto& job:jobs) job.cancel->store(true); }
        wake.notify_one(); if (worker.joinable()) worker.join();
    }
    Cancel submit(std::function<void(Cancel)> task) {
        auto token=std::make_shared<std::atomic_bool>(false);
        { std::lock_guard<std::mutex> lock(mutex);
          if (stopping) { token->store(true); return token; }
          jobs.push_back({std::move(task),token}); }
        wake.notify_one(); return token;
    }
};
inline Queue* activeForeground=nullptr;
inline Queue* activePrefetch=nullptr;
inline Queue& foreground() { static Queue worker; activeForeground=&worker; return worker; }
inline Queue& prefetch() { static Queue worker; activePrefetch=&worker; return worker; }
inline void shutdown() { if (activeForeground) activeForeground->stop(); if (activePrefetch) activePrefetch->stop(); }
}
