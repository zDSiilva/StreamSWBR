#pragma once
namespace brls { struct Logger {
    template<class... T> static void warning(const char*,T&&...) {}
    template<class... T> static void debug(const char*,T&&...) {}
    template<class... T> static void info(const char*,T&&...) {}
}; }
