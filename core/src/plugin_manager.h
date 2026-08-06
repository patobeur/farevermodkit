#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fmk {

struct PluginManifest {
    std::string id;
    std::string author;
    std::string name;
    std::string version;
    std::string api_version;
    std::string entry;
    std::string default_language;
    bool enabled_by_default = true;
    bool requires_game_world = false;
    std::filesystem::path directory;
    std::string error;
    bool valid = false;
};

class PluginManager {
public:
    explicit PluginManager(std::filesystem::path modules_root);

    // Discovers only modules/<author>/<mod>/manifest.json. No code is loaded.
    std::vector<PluginManifest> discover() const;

private:
    std::filesystem::path root_;
};

} // namespace fmk