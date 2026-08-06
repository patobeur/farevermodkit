#pragma once

namespace fmk {

// Read-only memory code reports diagnostics through this sink. The native
// host installs its file logger at startup; the default sink is debug-only.
using MemoryLogSink = void (*)(const char* message);

void set_memory_log_sink(MemoryLogSink sink);
void memory_log(const char* format, ...);

} // namespace fmk
