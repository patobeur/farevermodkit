#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "memory_log.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace fmk {
namespace {
std::atomic<MemoryLogSink> g_sink{nullptr};
}

void set_memory_log_sink(MemoryLogSink sink) {
    g_sink.store(sink, std::memory_order_release);
}

void memory_log(const char* format, ...) {
    char buffer[1024]{};
    va_list args;
    va_start(args, format);
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);

    if (const auto sink = g_sink.load(std::memory_order_acquire)) {
        sink(buffer);
        return;
    }
    OutputDebugStringA("[FareverModKit memory] ");
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
}
} // namespace fmk
