#pragma once

#include "memory/game_memory.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fmk {

class LuaRuntime {
public:
    LuaRuntime() = default;
    ~LuaRuntime();
    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;

    // Loads the configured Lua DLL and installs the restricted standard library.
    // Supplies the central read-only game-state gateway to Lua bindings.
    // The runtime never owns this pointer; PluginHost owns the instance.
    void set_memory_context(GameMemory* memory) { memory_ = memory; }
    GameMemory* memory_context() const { return memory_; }

    bool load_engine(const std::filesystem::path& dll_path, const std::string& expected_sha256);
    // Executes one plugin script inside the restricted state.
    bool execute_file(const std::filesystem::path& script);
    // Loads a flat JSON key/value language catalog for i18n().
    bool load_language_file(const std::filesystem::path& language_file);
    // Calls an optional lifecycle callback, such as on_render or on_shutdown.
    bool call_callback(const char* name);
    // Dispatches an event name; structured data is reserved and currently nil.
    bool call_event(const char* name);
    const std::string& last_error() const { return error_; }
    std::string translate(const std::string& key) const;

    const std::vector<std::string>& rendered_text() const { return rendered_text_; }
    void clear_rendered_text() { rendered_text_.clear(); }
    void capture_text(const char* text);

private:
    bool install_sandbox();
    void* library_ = nullptr;
    void* state_ = nullptr;
    bool sandbox_ready_ = false;
    std::string error_;
    std::unordered_map<std::string, std::string> translations_;
    std::vector<std::string> rendered_text_;
    GameMemory* memory_ = nullptr;
};

} // namespace fmk