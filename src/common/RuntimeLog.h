#pragma once
#include "DiagnosticLogging.h"
#include "RuntimePaths.h"
#include <fstream>
#include <mutex>
#include <string>

namespace kharvox {
// Startup acknowledgements are control signals consumed by the launcher.
// Keep them available without enabling per-frame diagnostic logging.
inline void writeRuntimeLog(const char* component, const std::string& text,
    bool operational = false) noexcept {
    if (!operational && !extendedDiagnosticsEnabled()) return;
    try {
        static std::mutex mutex;
        std::lock_guard<std::mutex> guard(mutex);
        const auto path = logPathA("KHARVOX.log");
        if (path.empty()) return;
        std::ofstream out(path, std::ios::app);
        SYSTEMTIME t{}; GetLocalTime(&t);
        out << '[' << t.wHour << ':' << t.wMinute << ':' << t.wSecond
            << '.' << t.wMilliseconds << "] " << component << ' ' << text << '\n';
    } catch (...) { /* Logging failure must not terminate the game. */ }
}
}
