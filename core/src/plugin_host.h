#pragma once

#include "lua_runtime.h"
#include "memory/game_memory.h"
#include "plugin_manager.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fmk {

struct PluginStatus {
    PluginManifest manifest;
    bool loaded = false;
    bool enabled = false;
    std::string error;
};

class PluginHost {
public:
    explicit PluginHost(std::filesystem::path modules_root);
    ~PluginHost();
    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    // Discovers modules and gives each valid module its own Lua state.
    bool load_all(const std::filesystem::path& lua_dll,
                  const std::string& lua_sha256,
                  const std::string& locale);
    bool set_enabled(const std::string& id, bool enabled);
    void dispatch_event(const char* name);
    void render();
    void shutdown();

    const std::vector<PluginStatus>& statuses() const { return statuses_; }
    std::vector<std::string> rendered_text(const std::string& id) const;

    // Central read-only game-state gateway for modules and future Lua bindings.
    GameMemory& memory() { return memory_; }
    const GameMemory& memory() const { return memory_; }

private:
    struct Entry {
        PluginStatus status;
        std::unique_ptr<LuaRuntime> runtime;
        bool initialized = false;
    };

    std::filesystem::path modules_root_;
    std::vector<Entry> entries_;
    std::vector<PluginStatus> statuses_;
    GameMemory memory_;
};

} // namespace fmk