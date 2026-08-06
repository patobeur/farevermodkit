#include "plugin_manager.h"

#include <fstream>
#include <regex>
#include <sstream>

namespace fmk {
namespace {

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::string json_string(const std::string& text, const char* key) {
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    return std::regex_search(text, match, pattern) ? match[1].str() : std::string();
}

bool json_bool(const std::string& text, const char* key, bool fallback) {
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) return fallback;
    return match[1].str() == "true";
}

bool safe_component(const std::string& value) {
    return !value.empty() && value != "." && value != ".." &&
           value.find("..") == std::string::npos &&
           value.find_first_of("\\/:*?\"<>|") == std::string::npos;
}

PluginManifest read_manifest(const std::filesystem::path& path) {
    PluginManifest out;
    out.directory = path.parent_path();
    const std::string text = read_text(path);
    if (text.empty()) { out.error = "manifest is empty or unreadable"; return out; }
    out.id = json_string(text, "id");
    out.author = json_string(text, "author");
    out.name = json_string(text, "name");
    out.version = json_string(text, "version");
    out.api_version = json_string(text, "apiVersion");
    out.entry = json_string(text, "entry");
    out.default_language = json_string(text, "defaultLanguage");
    out.enabled_by_default = json_bool(text, "enabledByDefault", true);
    out.requires_game_world = json_bool(text, "requiresGameWorld", false);
    if (!safe_component(out.id) || !safe_component(out.author) ||
        !safe_component(out.name) || !safe_component(out.version) ||
        !safe_component(out.entry) || out.api_version.empty() ||
        out.default_language.empty()) {
        out.error = "manifest has missing or unsafe fields";
        return out;
    }
    const auto entry = out.directory / std::filesystem::path(out.entry);
    const auto canonical_dir = std::filesystem::weakly_canonical(out.directory);
    const auto canonical_entry = std::filesystem::weakly_canonical(entry);
    const auto relative = std::filesystem::relative(canonical_entry, canonical_dir);
    if (relative.empty() || relative.native().starts_with(L"..")) {
        out.error = "entry escapes plugin directory";
        return out;
    }
    if (!std::filesystem::exists(entry)) {
        out.error = "entry file does not exist";
        return out;
    }
    out.valid = true;
    return out;
}

} // namespace

PluginManager::PluginManager(std::filesystem::path modules_root)
    : root_(std::move(modules_root)) {}

std::vector<PluginManifest> PluginManager::discover() const {
    std::vector<PluginManifest> found;
    if (!std::filesystem::exists(root_)) return found;
    for (const auto& author : std::filesystem::directory_iterator(root_)) {
        if (!author.is_directory()) continue;
        for (const auto& mod : std::filesystem::directory_iterator(author.path())) {
            if (!mod.is_directory()) continue;
            const auto manifest = mod.path() / "manifest.json";
            if (std::filesystem::exists(manifest)) found.push_back(read_manifest(manifest));
        }
    }
    return found;
}

} // namespace fmk