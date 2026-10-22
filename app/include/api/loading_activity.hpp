#pragma once
#include <atomic>
#include <memory>
namespace loading_activity {
inline std::atomic_uint pending{0};
struct Ticket {
    Ticket() { ++pending; }
    ~Ticket() { --pending; }
    Ticket(const Ticket&)=delete;
};
inline auto track() { return std::make_shared<Ticket>(); }
inline bool busy() { return pending.load()!=0; }
}
