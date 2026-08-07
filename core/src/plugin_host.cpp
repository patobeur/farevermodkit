#include "plugin_host.h"

#include <unordered_set>
#include "memory/memory_log.h"

namespace fmk {
namespace {

bool safe_locale(const std::string& locale) {
    return !locale.empty() && locale != "." && locale != ".." &&
           locale.find_first_of("\\/:*?\"<>|") == std::string::npos &&
           locale.find("..") == std::string::npos;
}



} // namespace

PluginHost::PluginHost(std::filesystem::path modules_root)
    : modules_root_(std::move(modules_root)) {}

PluginHost::~PluginHost() {
    shutdown();
}

bool PluginHost::load_all(const std::filesystem::path& lua_dll,
                          const std::string& lua_sha256,
                          const std::string& locale) {
    shutdown();
    entries_.clear();
    statuses_.clear();
    if (!safe_locale(locale)) return false;

    PluginManager manager(modules_root_);
    const auto manifests = manager.discover();
    std::unordered_set<std::string> ids;
    bool any_loaded = false;

    for (const auto& manifest : manifests) {
        Entry entry;
        entry.status.manifest = manifest;
        if (!manifest.valid) {
            entry.status.error = manifest.error;
            entries_.push_back(std::move(entry));
            continue;
        }
        if (!ids.insert(manifest.id).second) {
            entry.status.error = "duplicate plugin id";
            entries_.push_back(std::move(entry));
            continue;
        }

        entry.runtime = std::make_unique<LuaRuntime>();
        entry.runtime->set_memory_context(&memory_);
        if (!entry.runtime->load_engine(lua_dll, lua_sha256)) {
            entry.status.error = entry.runtime->last_error();
            entry.runtime.reset();
            entries_.push_back(std::move(entry));
            continue;
        }

        std::filesystem::path language = manifest.directory / "languages" / (locale + ".json");
        if (!std::filesystem::is_regular_file(language)) {
            language = manifest.directory / "languages" /
                       (manifest.default_language + ".json");
        }
        if (std::filesystem::is_regular_file(language) &&
            !entry.runtime->load_language_file(language)) {
            entry.status.error = entry.runtime->last_error();
            entry.runtime.reset();
            entries_.push_back(std::move(entry));
            continue;
        }
        const auto script = manifest.directory / std::filesystem::path(manifest.entry);
        if (!entry.runtime->execute_file(script)) {
            entry.status.error = entry.runtime->last_error();
            entry.runtime.reset();
            entries_.push_back(std::move(entry));
            continue;
        }

        entry.status.loaded = true;
        entry.status.enabled = false;
        any_loaded = true;
        if (manifest.enabled_by_default) {
            entry.status.enabled = true;
            if (!manifest.requires_game_world || memory_.available()) {
                if (!entry.runtime->call_callback("on_init")) {
                    entry.status.error = entry.runtime->last_error();
                    entry.status.loaded = false;
                    entry.runtime.reset();
                    entries_.push_back(std::move(entry));
                    continue;
                }
                entry.initialized = true;
            }
        }
        entries_.push_back(std::move(entry));
    }

    statuses_.reserve(entries_.size());
    for (const auto& entry : entries_) statuses_.push_back(entry.status);
    return any_loaded;
}

bool PluginHost::set_enabled(const std::string& id, bool enabled) {
    for (auto& entry : entries_) {
        if (entry.status.manifest.id != id) continue;
        if (!entry.runtime || !entry.status.loaded) return false;
        if (entry.status.enabled == enabled) return true;
        if (enabled) {
            entry.status.enabled = true;
            if (!entry.status.manifest.requires_game_world || memory_.available()) {
                if (!entry.runtime->call_callback("on_init")) {
                    entry.status.error = entry.runtime->last_error();
                    entry.status.enabled = false;
                    return false;
                }
                entry.initialized = true;
            }
        } else {
            entry.runtime->clear_rendered_text();
            if (entry.initialized && !entry.runtime->call_callback("on_shutdown")) {
                entry.status.error = entry.runtime->last_error();
            }
            entry.initialized = false;
            entry.status.enabled = false;
        }
        for (auto& status : statuses_) {
            if (status.manifest.id == id) status = entry.status;
        }
        return true;
    }
    return false;
}

void PluginHost::dispatch_event(const char* name) {
    for (auto& entry : entries_) {
        if (!entry.runtime || !entry.status.enabled || !entry.initialized) continue;
        if (!entry.runtime->call_event(name)) {
            entry.status.error = entry.runtime->last_error();
            entry.status.enabled = false;
            for (auto& status : statuses_) {
                if (status.manifest.id == entry.status.manifest.id) status = entry.status;
            }
        }
    }
}
void PluginHost::render() {
    for (auto& entry : entries_) {
        if (!entry.runtime || !entry.status.enabled) continue;
        if (entry.status.manifest.requires_game_world && !memory_.available()) {
            entry.runtime->clear_rendered_text();
            if (entry.initialized) {
                entry.runtime->call_callback("on_shutdown");
                entry.initialized = false;
            }
            continue;
        }
        if (!entry.initialized) {
            if (!entry.runtime->call_callback("on_init")) {
                entry.status.error = entry.runtime->last_error();
                entry.status.enabled = false;
                continue;
            }
            entry.initialized = true;
        }
        entry.runtime->clear_rendered_text();
        // Diagnostic guard removed.
        if (!entry.runtime->call_callback("on_render")) {
            entry.status.error = entry.runtime->last_error();
            memory_log("plugin %s render error: %s", entry.status.manifest.id.c_str(),
                       entry.status.error.c_str());
            entry.status.enabled = false;
            for (auto& status : statuses_) {
                if (status.manifest.id == entry.status.manifest.id) status = entry.status;
            }
        }
    }
}

void PluginHost::shutdown() {
    for (auto& entry : entries_) {
        if (!entry.runtime || !entry.status.enabled || !entry.initialized) continue;
        if (!entry.runtime->call_callback("on_shutdown")) {
            entry.status.error = entry.runtime->last_error();
        }
        entry.status.enabled = false;
        for (auto& status : statuses_) {
            if (status.manifest.id == entry.status.manifest.id) status = entry.status;
        }
    }
}

std::vector<std::string> PluginHost::rendered_text(const std::string& id) const {
    for (const auto& entry : entries_) {
        if (entry.status.manifest.id == id && entry.runtime) {
            return entry.runtime->rendered_text();
        }
    }
    return {};
}

std::vector<DrawCommand> PluginHost::draw_commands(const std::string& id) const {
    for (const auto& entry : entries_) {
        if (entry.status.manifest.id == id && entry.runtime)
            return entry.runtime->draw_commands();
    }
    return {};
}
std::pair<float, float> PluginHost::canvas_size(const std::string& id) const {
    for (const auto& entry : entries_) {
        if (entry.status.manifest.id == id && entry.runtime)
            return {entry.runtime->canvas_width(), entry.runtime->canvas_height()};
    }
    return {0.0f, 0.0f};
}
} // namespace fmk