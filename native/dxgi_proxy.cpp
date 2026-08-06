#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dxgi.h>
#include <bcrypt.h>
#include <share.h>
#include <stdarg.h>
#include <stdio.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "dxgi_wrap.h"
#include "overlay.h"
#include "atlas_ui.h"
#include "input.h"
#include "navigator.h"
#include "plugin_host.h"
#include "memory/memory_log.h"

namespace {
HMODULE g_real = nullptr;
HMODULE g_instance = nullptr;
FILE* g_log = nullptr;
CRITICAL_SECTION g_log_lock;
std::unique_ptr<fmk::PluginHost> g_plugins;
volatile LONG g_atlas_ready = 0;
int g_fmk_icon_texture = -1;
int g_close_icon_texture = -1;
int g_module_on_texture = -1;
int g_module_off_texture = -1;
int g_module_options_texture = -1;
int g_bossrun_enabled_texture = -1;
int g_bossrun_disabled_texture = -1;
std::unordered_map<std::string, int> g_author_icon_textures;
std::unordered_map<std::string, int> g_module_icon_textures;

void log_line(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);
    OutputDebugStringA("[FareverModKit] ");
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
    if (!g_log) return;
    EnterCriticalSection(&g_log_lock);
    fprintf(g_log, "%s\n", buffer);
    fflush(g_log);
    LeaveCriticalSection(&g_log_lock);
}

void memory_log_sink(const char* message) {
    log_line("%s", message ? message : "");
}

HMODULE real_dxgi() {
    if (g_real) return g_real;
    wchar_t path[MAX_PATH];
    const UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (!length || length >= MAX_PATH - 10) return nullptr;
    wcscat_s(path, L"\\dxgi.dll");
    g_real = LoadLibraryW(path);
    log_line("real dxgi.dll: %s", g_real ? "loaded" : "FAILED");
    return g_real;
}

template <typename T>
T forward_export(const char* name) {
    const auto module = real_dxgi();
    return module ? reinterpret_cast<T>(GetProcAddress(module, name)) : nullptr;
}

template <typename T>
HRESULT create_factory(const char* name, T fn, REFIID riid, void** output) {
    (void)name;
    if (!fn) return E_FAIL;
    const HRESULT hr = fn(riid, output);
    if (SUCCEEDED(hr) && output && *output) *output = fmk::dxgi_wrap_factory(*output, riid);
    return hr;
}

std::filesystem::path find_upward(const std::filesystem::path& relative) {
    wchar_t path[MAX_PATH];
    if (!g_instance || !GetModuleFileNameW(g_instance, path, MAX_PATH)) return {};
    auto current = std::filesystem::path(path).parent_path();
    for (int i = 0; i < 8; ++i) {
        const auto candidate = current / relative;
        if (std::filesystem::exists(candidate)) return candidate;
        const auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return {};
}

std::string configured_lua_hash(const std::filesystem::path& config) {
    std::ifstream input(config, std::ios::binary);
    if (!input) return {};
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    std::smatch match;
    const std::regex pattern("\\\"sha256\\\"\\s*:\\s*\\\"([0-9A-Fa-f]{64})\\\"");
    return std::regex_search(text, match, pattern) ? match[1].str() : std::string();
}

std::string sha256_file_hex(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_length = 0;
    DWORD result_length = 0;
    std::vector<unsigned char> object;
    std::vector<unsigned char> digest(32);
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&object_length), sizeof(object_length),
                          &result_length, 0) != 0) {
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }
    object.resize(object_length);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_length,
                         nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }
    std::vector<char> buffer(64 * 1024);
    while (input.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) ||
           input.gcount() > 0) {
        const auto bytes = static_cast<ULONG>(input.gcount());
        if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), bytes, 0) != 0) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return {};
        }
    }
    const NTSTATUS finish = BCryptFinishHash(hash, digest.data(),
                                              static_cast<ULONG>(digest.size()), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (finish != 0) return {};
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        output << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return output.str();
}
struct NativeConfig {
    bool overlay_enabled = false;
    DWORD overlay_delay_ms = 20000;
    int overlay_stage = 1;
    bool memory_enabled = false;
    bool memory_allow_scan = false;
};

NativeConfig read_native_config() {
    NativeConfig config;
    const auto path = find_upward("farevermodkit/config/native.json");
    std::ifstream input(path, std::ios::binary);
    if (!input) return config;
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    std::smatch match;
    const std::regex enabled_pattern("\\\"enabled\\\"\\s*:\\s*(true|false)");
    if (std::regex_search(text, match, enabled_pattern))
        config.overlay_enabled = match[1].str() == "true";
    const std::regex delay_pattern("\\\"startupDelayMs\\\"\\s*:\\s*(\\d+)");
    if (std::regex_search(text, match, delay_pattern)) {
        try {
            const auto value = std::stoul(match[1].str());
            config.overlay_delay_ms = (DWORD)std::min<unsigned long>(value, 120000UL);
        } catch (...) {
            config.overlay_delay_ms = 20000;
        }
    }
    if (config.overlay_delay_ms < 20000) config.overlay_delay_ms = 20000;
    const std::regex stage_pattern("\\\"stage\\\"\\s*:\\s*(\\d+)");
    if (std::regex_search(text, match, stage_pattern)) {
        try {
            config.overlay_stage = std::stoi(match[1].str());
        } catch (...) {
            config.overlay_stage = 1;
        }
    }
    if (config.overlay_stage < 1 || config.overlay_stage > 4) config.overlay_stage = 1;
    const std::regex memory_enabled_pattern("\\\"memoryEnabled\\\"\\s*:\\s*(true|false)");
    if (std::regex_search(text, match, memory_enabled_pattern))
        config.memory_enabled = match[1].str() == "true";
    const std::regex memory_scan_pattern("\\\"memoryAllowScan\\\"\\s*:\\s*(true|false)");
    if (std::regex_search(text, match, memory_scan_pattern))
        config.memory_allow_scan = match[1].str() == "true";
    return config;
}

void load_plugins() {
    const auto modules = find_upward("farevermodkit/modules");
    const auto lua_dll = find_upward("farevermodkit/third_party/lua/bin/lua54.dll");
    const auto config = find_upward("farevermodkit/config/lua-runtime.json");
    const auto hash = configured_lua_hash(config);
    if (modules.empty() || lua_dll.empty() || hash.empty()) {
        log_line("plugins: test layout not found; static overlay remains active");
        return;
    }
    g_plugins = std::make_unique<fmk::PluginHost>(modules);
    if (!g_plugins->load_all(lua_dll, hash, "en-US")) {
        log_line("plugins: no module loaded");
    }
    for (const auto& status : g_plugins->statuses()) {
        log_line("plugins: %s loaded=%d enabled=%d error=%s",
                 status.manifest.id.c_str(), status.loaded ? 1 : 0,
                 status.enabled ? 1 : 0, status.error.c_str());
    }
}

DWORD WINAPI early_memory_worker(void*) {
    const NativeConfig native = read_native_config();
    if (!native.memory_enabled || !g_plugins) return 0;
    const auto boot = find_upward("hlboot.dat");
    const auto build_hash = sha256_file_hex(boot);
    if (build_hash.empty()) {
        log_line("memory: cannot hash hlboot.dat; reads disabled");
        return 0;
    }
    if (!g_plugins->memory().configure_build_hash(build_hash)) {
        log_line("memory: build hash mismatch; reads disabled");
        return 0;
    }

    // Blaakan's proven startup cycle: try the cheap App.inst path every
    // three seconds. Never permit the fallback instance scan.
    for (int app_try = 0; app_try < 40; ++app_try) {
        if (g_plugins->memory().probe(false)) {
            log_line("memory: early App.inst discovery completed");
            return 0;
        }
        Sleep(3000);
    }
    log_line("memory: early App.inst discovery timed out without scanning instances");
    return 0;
}

void load_ui_icon_textures() {
    const auto fmk_icon = find_upward("farevermodkit/assets/fmk/icon.png");
    if (!fmk_icon.empty()) g_fmk_icon_texture = fmk::overlay_load_image(fmk_icon.u8string().c_str());
    const auto close_icon = find_upward("farevermodkit/assets/fmk/close.png");
    if (!close_icon.empty()) {
        g_close_icon_texture = fmk::overlay_load_image(close_icon.u8string().c_str());
        fmk::atlas_ui_set_close_texture(g_close_icon_texture);
    }    const auto on_icon = find_upward("farevermodkit/assets/fmk/on.png");
    const auto off_icon = find_upward("farevermodkit/assets/fmk/off.png");
    const auto options_icon = find_upward("farevermodkit/assets/fmk/options.png");
    if (!on_icon.empty())
        g_module_on_texture = fmk::overlay_load_image(on_icon.u8string().c_str());
    if (!off_icon.empty())
        g_module_off_texture = fmk::overlay_load_image(off_icon.u8string().c_str());
    if (!options_icon.empty())
        g_module_options_texture = fmk::overlay_load_image(options_icon.u8string().c_str());
    if (!g_plugins) return;
    for (const auto& status : g_plugins->statuses()) {
        const std::string& author = status.manifest.author;
        if (!g_author_icon_textures.count(author)) {
            const auto path = status.manifest.directory.parent_path() / "icon.png";
            g_author_icon_textures[author] = std::filesystem::is_regular_file(path)
                ? fmk::overlay_load_image(path.u8string().c_str()) : -1;
        }
        const auto path = status.manifest.directory / "icon.png";
        g_module_icon_textures[status.manifest.id] = std::filesystem::is_regular_file(path)
            ? fmk::overlay_load_image(path.u8string().c_str()) : -1;
        if (status.manifest.id == "patobeur.bossrun") {
            const auto enabled = status.manifest.directory / "assets" / "enabled.png";
            const auto disabled = status.manifest.directory / "assets" / "disabled.png";
            if (std::filesystem::is_regular_file(enabled))
                g_bossrun_enabled_texture = fmk::overlay_load_image(enabled.u8string().c_str());
            if (std::filesystem::is_regular_file(disabled))
                g_bossrun_disabled_texture = fmk::overlay_load_image(disabled.u8string().c_str());
        }
    }
}
DWORD WINAPI atlas_worker(void*) {
    for (int i = 0; i < 300 && !fmk::overlay_ready(); ++i) Sleep(100);
    if (!fmk::overlay_ready()) {
        log_line("atlas: overlay never became ready");
        return 0;
    }
    for (int i = 0; i < 100; ++i) {
        void* hwnd = fmk::overlay_game_hwnd();
        if (hwnd && fmk::input_install(hwnd)) break;
        Sleep(100);
    }
    load_ui_icon_textures();
    if (!fmk::atlas_ui_init()) {
        log_line("atlas: Blaakan UI initialization failed");
        return 0;
    }
    fmk::nav_init();
    InterlockedExchange(&g_atlas_ready, 1);
    log_line("atlas: Blaakan UI ready");
    DWORD last_atlas_update = 0;
    while (g_plugins) {
        const auto status = g_plugins->memory().status();
        fmk::atlas_ui_set_in_world(status.in_world);
        if (status.in_world) {
            double x=0,y=0,z=0,rot=0;
            const bool pose = g_plugins->memory().read_hero_pose(&x,&y,&z,&rot);
            fmk::nav_set_hero_pose(pose,x,y,z,rot);
            double px=0,py=0,pz=0,tx=0,ty=0,tz=0;
            const bool camera = g_plugins->memory().read_camera(&px,&py,&pz,&tx,&ty,&tz);
            fmk::nav_set_camera(camera,px,py,pz,tx,ty,tz);
        } else {
            fmk::nav_set_hero_pose(false,0,0,0,0);
            fmk::nav_set_camera(false,0,0,0,0,0,0);
        }
        const DWORD now = GetTickCount();
        if (status.in_world && now - last_atlas_update >= 1000) {
            last_atlas_update = now;
            fmk::Collection collection{};
            fmk::Inventories inventories{};
            std::vector<fmk::UnitProgress> units;
            std::vector<fmk::JobState> jobs;
            fmk::RuneState runes{};
            fmk::CompletionState completion{};
            std::vector<fmk::WeaponMastery> mastery;
            g_plugins->memory().read_collection(&collection);
            g_plugins->memory().read_inventories(&inventories);
            g_plugins->memory().read_unit_progress(&units);
            g_plugins->memory().read_jobs(&jobs);
            g_plugins->memory().read_runes(&runes);
            g_plugins->memory().read_completion(&completion);
            g_plugins->memory().read_weapon_mastery(&mastery);
            const bool save_requested = fmk::atlas_ui_take_save_request();
            fmk::atlas_ui_update(collection, inventories, units, jobs, runes,
                                 completion, mastery, save_requested);
            if (save_requested) fmk::atlas_ui_mark_saved();
            fmk::atlas_ui_tick();
        }
        fmk::nav_tick();
        Sleep(50);
    }    return 0;
}
DWORD WINAPI overlay_worker(void*) {
    Sleep(3000);
    load_plugins();
    HANDLE memory_thread = CreateThread(nullptr, 0, early_memory_worker, nullptr, 0, nullptr);
    if (memory_thread) CloseHandle(memory_thread);
    const NativeConfig native = read_native_config();
    if ((!native.overlay_enabled || native.overlay_stage < 1) && !native.memory_enabled) {
        log_line("overlay disabled: safe plugin-load mode");
        return 0;
    }
    Sleep(native.overlay_delay_ms);
    if (!native.overlay_enabled || native.overlay_stage < 1) {
        log_line("overlay disabled: safe plugin-load mode");
        return 0;
    }
    log_line("overlay: startup delay elapsed; installing hooks");
    if (native.overlay_stage >= 4) {
        fmk::overlay_set_draw([](float width, float height) {
            struct WindowState { float x, y; };
            struct IconState { float x, y; bool moved; };
            static bool manager_open = false;
            static bool previous_click = false;
            static float panel_x = 0.0f;
            static float panel_y = 146.0f;
            static IconState fmk_icon{0.0f, 146.0f, false};
            static bool layout_initialized = false;
            static std::size_t module_scroll = 0;
            static std::unordered_map<std::string, WindowState> plugin_windows;
            static std::unordered_map<std::string, IconState> plugin_icons;
            static std::unordered_map<std::string, bool> plugin_open;
            static std::string dragging;
            static float drag_dx = 0.0f;
            static float drag_dy = 0.0f;
            static bool state_loaded = false;
            static std::filesystem::path state_path;
            static std::unordered_map<std::string, bool> saved_enabled;
            static bool atlas_input_open = false;
            bool state_dirty = false;

            if (!state_loaded) {
                const auto native_config = find_upward("farevermodkit/config/native.json");
                if (!native_config.empty()) state_path = native_config.parent_path() / "ui-state.json";
                if (!state_path.empty() && std::filesystem::is_regular_file(state_path)) {
                    std::ifstream input(state_path, std::ios::binary);
                    const std::string saved((std::istreambuf_iterator<char>(input)),
                                            std::istreambuf_iterator<char>());
                    auto read_number = [&](const char* key, float fallback) {
                        const std::regex pattern(std::string("\\\"") + key +
                            "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
                        std::smatch match;
                        return std::regex_search(saved, match, pattern)
                            ? std::stof(match[1].str()) : fallback;
                    };
                    auto read_bool = [&](const char* key, bool fallback) {
                        const std::regex pattern(std::string("\\\"") + key +
                            "\\\"\\s*:\\s*(true|false)");
                        std::smatch match;
                        return std::regex_search(saved, match, pattern)
                            ? match[1].str() == "true" : fallback;
                    };
                    manager_open = read_bool("managerOpen", manager_open);
                    panel_x = read_number("panelX", panel_x);
                    panel_y = read_number("panelY", panel_y);
                    fmk_icon.x = read_number("fmkX", fmk_icon.x);
                    fmk_icon.y = read_number("fmkY", fmk_icon.y);
                    fmk_icon.moved = read_bool("fmkMoved", true);
                    module_scroll = (std::size_t)std::max(0.0f,
                        read_number("moduleScroll", (float)module_scroll));
                    const std::regex module_pattern(
                        "\\{\\s*\\\"id\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"\\s*,\\s*"
                        "\\\"enabled\\\"\\s*:\\s*(true|false)\\s*,\\s*"
                        "\\\"iconX\\\"\\s*:\\s*(-?[0-9.]+)\\s*,\\s*"
                        "\\\"iconY\\\"\\s*:\\s*(-?[0-9.]+)\\s*,\\s*"
                        "\\\"iconMoved\\\"\\s*:\\s*(true|false)\\s*,\\s*"
                        "\\\"windowX\\\"\\s*:\\s*(-?[0-9.]+)\\s*,\\s*"
                        "\\\"windowY\\\"\\s*:\\s*(-?[0-9.]+)\\s*,\\s*"
                        "\\\"open\\\"\\s*:\\s*(true|false)\\s*\\}");
                    for (std::sregex_iterator it(saved.begin(), saved.end(), module_pattern), end;
                         it != end; ++it) {
                        const auto& match = *it;
                        const std::string id = match[1].str();
                        saved_enabled[id] = match[2].str() == "true";
                        plugin_icons[id] = {std::stof(match[3].str()), std::stof(match[4].str()),
                                            match[5].str() == "true"};
                        plugin_windows[id] = {std::stof(match[6].str()), std::stof(match[7].str())};
                        plugin_open[id] = match[8].str() == "true";
                    }
                    layout_initialized = true;
                }
                if (g_plugins) {
                    for (const auto& [id, enabled] : saved_enabled) {
                        g_plugins->set_enabled(id, enabled);
                        log_line("ui-state: restored %s enabled=%d", id.c_str(), enabled ? 1 : 0);
                    }
                }
                state_loaded = true;
            }

            if (g_plugins) {
                g_plugins->memory().refresh();
                g_plugins->render();
            }
            if (InterlockedCompareExchange(&g_atlas_ready, 0, 0))
                fmk::nav_draw(width, height);
            POINT cursor{};
            HWND window = reinterpret_cast<HWND>(fmk::overlay_game_hwnd());
            const bool cursor_valid = window && GetCursorPos(&cursor) &&
                                      ScreenToClient(window, &cursor);
            const bool click_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool clicked = click_down && !previous_click;
            const bool released = !click_down && previous_click;
            auto in_rect = [&](float x, float y, float w, float h) {
                return cursor_valid && cursor.x >= (LONG)x && cursor.x < (LONG)(x + w) &&
                       cursor.y >= (LONG)y && cursor.y < (LONG)(y + h);
            };
            auto clamp_window = [&](float& x, float& y, float w, float h) {
                x = std::clamp(x, 0.0f, std::max(0.0f, width - w));
                y = std::clamp(y, 0.0f, std::max(0.0f, height - h));
            };
            auto start_drag = [&](const std::string& id, float x, float y) {
                dragging = id;
                drag_dx = (float)cursor.x - x;
                drag_dy = (float)cursor.y - y;
            };
            auto button = [](float x, float y, float w, const char* label,
                             bool hot, fmk::Color color) {
                const fmk::Color fill = hot
                    ? fmk::Color{color.r + 0.10f, color.g + 0.10f, color.b + 0.10f, 1.0f}
                    : color;
                fmk::draw_rect(x, y, w, 26.0f, fill);
                fmk::draw_rect_outline(x, y, w, 26.0f, 1.0f,
                                       {0.75f, 0.85f, 0.90f, 1.0f});
                fmk::draw_text(x + 8.0f, y + 5.0f, 14.0f,
                               {1.0f, 1.0f, 1.0f, 1.0f}, label);
            };
            auto draw_icon = [&](float x, float y, int texture, const char* label, bool hot) {
                constexpr float size = 58.0f;
                fmk::draw_rect(x, y, size, size,
                               hot ? fmk::Color{0.12f, 0.55f, 0.65f, 0.98f}
                                   : fmk::Color{0.08f, 0.30f, 0.38f, 0.96f});
                if (texture >= 1)
                    fmk::draw_image(texture, x + 3, y + 3, size - 6, size - 6,
                                    0, 0, 1, 1, {1,1,1,1});
                else
                    fmk::draw_text(x + 7.0f, y + 19.0f, 17.0f,
                                   {1.0f, 1.0f, 1.0f, 1.0f}, label);
                fmk::draw_rect_outline(x, y, size, size, 2.0f,
                                       {0.35f, 0.85f, 0.95f, 1.0f});
            };

            const auto& statuses = g_plugins ? g_plugins->statuses()
                                              : std::vector<fmk::PluginStatus>{};
            std::vector<const fmk::PluginStatus*> ordered_statuses;
            ordered_statuses.reserve(statuses.size());
            for (const auto& status : statuses) ordered_statuses.push_back(&status);
            std::sort(ordered_statuses.begin(), ordered_statuses.end(),
                      [](const fmk::PluginStatus* a, const fmk::PluginStatus* b) {
                if (a->manifest.author != b->manifest.author)
                    return a->manifest.author < b->manifest.author;
                return a->manifest.name < b->manifest.name;
            });
            const bool world_available = g_plugins && g_plugins->memory().available();
            auto save_state = [&]() {
                if (state_path.empty()) return;
                std::ofstream output(state_path, std::ios::binary | std::ios::trunc);
                if (!output) return;
                output << std::fixed << std::setprecision(1);
                output << "{\n"
                       << "  \"managerOpen\": " << (manager_open ? "true" : "false") << ",\n"
                       << "  \"panelX\": " << panel_x << ",\n"
                       << "  \"panelY\": " << panel_y << ",\n"
                       << "  \"fmkX\": " << fmk_icon.x << ",\n"
                       << "  \"fmkY\": " << fmk_icon.y << ",\n"
                       << "  \"fmkMoved\": " << (fmk_icon.moved ? "true" : "false") << ",\n"
                       << "  \"moduleScroll\": " << module_scroll << ",\n"
                       << "  \"modules\": [\n";
                bool first = true;
                const auto& current_statuses = g_plugins->statuses();
                for (const auto& status : current_statuses) {
                    if (!first) output << ",\n";
                    first = false;
                    const auto icon_it = plugin_icons.find(status.manifest.id);
                    const auto window_it = plugin_windows.find(status.manifest.id);
                    const IconState icon = icon_it != plugin_icons.end()
                        ? icon_it->second : IconState{0, 0, false};
                    const WindowState win = window_it != plugin_windows.end()
                        ? window_it->second : WindowState{0, 0};
                    output << "    {\"id\":\"" << status.manifest.id
                           << "\",\"enabled\":" << (status.enabled ? "true" : "false")
                           << ",\"iconX\":" << icon.x << ",\"iconY\":" << icon.y
                           << ",\"iconMoved\":" << (icon.moved ? "true" : "false")
                           << ",\"windowX\":" << win.x << ",\"windowY\":" << win.y
                           << ",\"open\":" << (plugin_open[status.manifest.id] ? "true" : "false")
                           << "}";
                }
                output << "\n  ]\n}\n";
            };
            constexpr float panel_width = 650.0f;
            if (!layout_initialized && width > panel_width + 90.0f) {
                fmk_icon.x = width - 76.0f;
                fmk_icon.y = std::clamp(146.0f, 0.0f, std::max(0.0f, height - 58.0f));
                panel_x = std::max(0.0f, fmk_icon.x - panel_width - 8.0f);
                panel_y = fmk_icon.y;
                layout_initialized = true;
            }
            std::size_t enabled_index = 0;
            for (const auto& status : statuses) {
                if (!status.enabled || (status.manifest.requires_game_world && !world_available)) {
                    plugin_open[status.manifest.id] = false;
                    continue;
                }
                const float default_icon_x = fmk_icon.x;
                const float default_icon_y = fmk_icon.y + 68.0f * (float)(enabled_index + 1);
                auto [icon_it, icon_inserted] = plugin_icons.try_emplace(
                    status.manifest.id, IconState{default_icon_x, default_icon_y, false});
                if (!icon_inserted && !icon_it->second.moved) {
                    icon_it->second.x = default_icon_x;
                    icon_it->second.y = default_icon_y;
                }
                plugin_windows.try_emplace(status.manifest.id,
                    WindowState{std::max(20.0f, panel_x - 490.0f),
                                panel_y + 40.0f * (float)enabled_index});
                plugin_open.try_emplace(status.manifest.id,
                    status.manifest.id == "patobeur.bossrun");
                ++enabled_index;
            }

            std::string icon_tooltip;
            constexpr float icon_size = 58.0f;
            const bool fmk_hot = in_rect(fmk_icon.x, fmk_icon.y, icon_size, icon_size);
            if (clicked && fmk_hot) {
                fmk_icon.moved = false;
                start_drag("@fmk", fmk_icon.x, fmk_icon.y);
            }
            if (clicked && dragging.empty()) {
                for (const auto& status : statuses) {
                    if (!status.enabled || (status.manifest.requires_game_world && !world_available)) continue;
                    auto& icon = plugin_icons[status.manifest.id];
                    if (in_rect(icon.x, icon.y, icon_size, icon_size)) {
                        plugin_open[status.manifest.id] = !plugin_open[status.manifest.id];
                        state_dirty = true;
                        log_line("ui: %s open=%d", status.manifest.id.c_str(),
                                 plugin_open[status.manifest.id] ? 1 : 0);
                        icon.moved = false;
                        start_drag("@" + status.manifest.id, icon.x, icon.y);
                        break;
                    }
                }
            }
            if (click_down && cursor_valid && dragging == "@fmk") {
                const float nx = std::clamp((float)cursor.x - drag_dx, 0.0f,
                                            std::max(0.0f, width - icon_size));
                const float ny = std::clamp((float)cursor.y - drag_dy, 0.0f,
                                            std::max(0.0f, height - icon_size));
                if (std::abs(nx - fmk_icon.x) > 2.0f || std::abs(ny - fmk_icon.y) > 2.0f)
                    fmk_icon.moved = true;
                fmk_icon.x = nx; fmk_icon.y = ny;
            }
            if (click_down && cursor_valid && (!dragging.empty() && dragging[0] == '@') &&
                dragging != "@fmk") {
                const std::string id = dragging.substr(1);
                auto it = plugin_icons.find(id);
                if (it != plugin_icons.end()) {
                    auto& icon = it->second;
                    const float nx = std::clamp((float)cursor.x - drag_dx, 0.0f,
                                                std::max(0.0f, width - icon_size));
                    const float ny = std::clamp((float)cursor.y - drag_dy, 0.0f,
                                                std::max(0.0f, height - icon_size));
                    if (std::abs(nx - icon.x) > 8.0f || std::abs(ny - icon.y) > 8.0f)
                        icon.moved = true;
                    icon.x = nx; icon.y = ny;
                }
            }
            if (released && dragging == "@fmk" && !fmk_icon.moved) {
                manager_open = !manager_open;
                state_dirty = true;
            }
            if (released && (!dragging.empty() && dragging[0] == '@') && dragging != "@fmk") {
                const std::string id = dragging.substr(1);
                auto it = plugin_icons.find(id);
                // Window toggling happens on mouse-down; release only ends dragging.
            }

            draw_icon(fmk_icon.x, fmk_icon.y, g_fmk_icon_texture, "FMK", fmk_hot);
            if (fmk_hot) icon_tooltip = "FareverModKit - cliquer pour ouvrir/fermer les modules";
            for (const auto& status : statuses) {
                if (!status.enabled || (status.manifest.requires_game_world && !world_available)) continue;
                const auto& icon = plugin_icons[status.manifest.id];
                std::string label = status.manifest.id == "blaakan.inventory"
                    ? "INV" : status.manifest.name.substr(0, 3);
                std::transform(label.begin(), label.end(), label.begin(),
                               [](unsigned char c) { return (char)std::toupper(c); });
                const auto texture_it = g_module_icon_textures.find(status.manifest.id);
                const int texture = texture_it != g_module_icon_textures.end() ? texture_it->second : -1;
                const bool module_hot = in_rect(icon.x, icon.y, icon_size, icon_size);
                draw_icon(icon.x, icon.y, texture, label.c_str(), module_hot);
                if (module_hot) {
                    icon_tooltip = status.manifest.name + " (" + status.manifest.author +
                                   ") - cliquer pour ouvrir/fermer";
                }
            }

            const float row_height = 38.0f;
            const float author_height = 34.0f;
            constexpr std::size_t max_visible_modules = 8;
            const std::size_t max_scroll = ordered_statuses.size() > max_visible_modules
                ? ordered_statuses.size() - max_visible_modules : 0;
            module_scroll = std::min(module_scroll, max_scroll);
            const std::size_t list_end = std::min(ordered_statuses.size(),
                                                   module_scroll + max_visible_modules);
            const std::size_t visible_modules = list_end - module_scroll;
            std::size_t visible_authors = 0;
            std::string counted_author;
            for (std::size_t i = module_scroll; i < list_end; ++i) {
                if (ordered_statuses[i]->manifest.author != counted_author) {
                    counted_author = ordered_statuses[i]->manifest.author;
                    ++visible_authors;
                }
            }
            const bool can_scroll = ordered_statuses.size() > max_visible_modules;
            const float panel_height = 112.0f + (float)visible_modules * row_height +
                                       (float)visible_authors * author_height +
                                       (can_scroll ? 36.0f : 0.0f);
            if (manager_open) {
                const float manager_close_x = panel_x + panel_width - 30.0f;
                const float manager_close_y = panel_y + 6.0f;
                const bool manager_close_hot = in_rect(manager_close_x, manager_close_y, 22.0f, 22.0f);
                if (clicked && dragging.empty() && manager_close_hot) {
                    manager_open = false;
                    state_dirty = true;
                }
                if (clicked && dragging.empty() && !manager_close_hot &&
                    in_rect(panel_x, panel_y, panel_width, 34.0f))
                    start_drag("$manager", panel_x, panel_y);
                if (click_down && cursor_valid && dragging == "$manager") {
                    panel_x = (float)cursor.x - drag_dx;
                    panel_y = (float)cursor.y - drag_dy;
                    clamp_window(panel_x, panel_y, panel_width, panel_height);
                }
                fmk::draw_rect(panel_x, panel_y, panel_width, panel_height,
                               {0.05f, 0.08f, 0.10f, 0.94f});
                fmk::draw_rect(panel_x, panel_y, panel_width, 34.0f,
                               {0.08f, 0.13f, 0.17f, 0.98f});
                fmk::draw_rect_outline(panel_x, panel_y, panel_width, panel_height, 2.0f,
                                       {0.25f, 0.75f, 0.95f, 1.0f});
                if (g_fmk_icon_texture >= 1)
                    fmk::draw_image(g_fmk_icon_texture, panel_x + 7, panel_y + 3,
                                    28, 28, 0, 0, 1, 1, {1,1,1,1});
                fmk::draw_text(panel_x + (g_fmk_icon_texture >= 1 ? 44.0f : 20.0f),
                               panel_y + 8.0f, 24.0f,
                               {1.0f, 1.0f, 1.0f, 1.0f}, "FareverModKit");                if (g_close_icon_texture >= 1)
                    fmk::draw_image(g_close_icon_texture, manager_close_x, manager_close_y,
                                    22, 22, 0, 0, 1, 1,
                                    manager_close_hot
                                        ? fmk::Color{1.0f,0.72f,0.72f,1.0f}
                                        : fmk::Color{1.0f,1.0f,1.0f,1.0f});
                else
                    fmk::draw_text(manager_close_x + 5, manager_close_y + 1, 16,
                                   {1,1,1,1}, "x");
                fmk::draw_text(panel_x + 20.0f, panel_y + 42.0f, 15.0f,
                               {0.65f, 0.75f, 0.82f, 1.0f},
                               "Activer un mod affiche son icone");
                float y = panel_y + 70.0f;
                std::string visible_author;
                for (std::size_t module_index = module_scroll;
                     module_index < list_end; ++module_index) {
                    const auto& status = *ordered_statuses[module_index];
                    if (status.manifest.author != visible_author) {
                        visible_author = status.manifest.author;
                        fmk::draw_rect(panel_x + 10, y - 5, panel_width - 20, 29,
                                       {0.075f, 0.11f, 0.15f, 0.98f});
                        const auto author_icon = g_author_icon_textures.find(visible_author);
                        if (author_icon != g_author_icon_textures.end() && author_icon->second >= 1)
                            fmk::draw_image(author_icon->second, panel_x + 16, y - 3,
                                            25, 25, 0, 0, 1, 1, {1,1,1,1});
                        fmk::draw_text(panel_x + 50, y + 1, 17,
                                       {0.78f, 0.86f, 0.94f, 1}, visible_author.c_str());
                        y += author_height;
                    }
                    const float toggle_x = panel_x + 548.0f;
                    const float options_x = panel_x + 590.0f;
                    const bool hot_toggle = in_rect(toggle_x, y - 6.0f, 28.0f, 28.0f);
                    const bool hot_options = in_rect(options_x, y - 6.0f, 28.0f, 28.0f);
                    const char* state = status.enabled ? "active" : "desactive";
                    const std::string line = status.manifest.name + " - " + state;
                    const fmk::Color color = status.loaded && status.enabled
                        ? fmk::Color{0.45f, 0.90f, 0.65f, 1.0f}
                        : fmk::Color{0.95f, 0.55f, 0.45f, 1.0f};
                    const auto module_icon = g_module_icon_textures.find(status.manifest.id);
                    if (module_icon != g_module_icon_textures.end() && module_icon->second >= 1)
                        fmk::draw_image(module_icon->second, panel_x + 18, y - 5,
                                        26, 26, 0, 0, 1, 1, {1,1,1,1});
                    fmk::draw_text(panel_x + 54.0f, y, 16.0f, color, line.c_str());
                    const int toggle_texture = status.enabled
                        ? g_module_on_texture : g_module_off_texture;
                    if (toggle_texture >= 1)
                        fmk::draw_image(toggle_texture, toggle_x, y - 6.0f, 28, 28,
                                        0, 0, 1, 1,
                                        hot_toggle ? fmk::Color{1.0f,1.0f,0.78f,1.0f}
                                                   : fmk::Color{1,1,1,1});
                    else
                        button(toggle_x, y - 5.0f, 28.0f, status.enabled ? "-" : "+",
                               hot_toggle, {0.20f, 0.35f, 0.30f, 1.0f});
                    if (g_module_options_texture >= 1)
                        fmk::draw_image(g_module_options_texture, options_x, y - 6.0f, 28, 28,
                                        0, 0, 1, 1,
                                        hot_options ? fmk::Color{1.0f,1.0f,0.78f,1.0f}
                                                    : fmk::Color{1,1,1,1});
                    else
                        button(options_x, y - 5.0f, 28.0f, "?", hot_options,
                               {0.20f, 0.28f, 0.36f, 1.0f});
                    if (hot_toggle)
                        icon_tooltip = status.enabled ? "Desactiver " + status.manifest.name
                                                      : "Activer " + status.manifest.name;
                    else if (hot_options)
                        icon_tooltip = "Options de " + status.manifest.name;
                    if (clicked && dragging.empty() && hot_toggle) {
                        const bool desired = !status.enabled;
                        const bool changed = g_plugins->set_enabled(status.manifest.id, desired);
                        plugin_open[status.manifest.id] = desired &&
                            status.manifest.id == "patobeur.bossrun";
                        state_dirty = true;
                        log_line("ui: module %s %s (%s)", status.manifest.id.c_str(),
                                 desired ? "activation" : "desactivation",
                                 changed ? "ok" : "failed");
                    } else if (clicked && dragging.empty() && hot_options) {
                        log_line("ui: options requested for %s (not implemented)",
                                 status.manifest.id.c_str());
                    }
                    y += row_height;
                }
                if (can_scroll) {
                    const float footer_y = panel_y + panel_height - 31.0f;
                    const bool hot_up = in_rect(panel_x + 382.0f, footer_y, 96.0f, 26.0f);
                    const bool hot_down = in_rect(panel_x + 492.0f, footer_y, 96.0f, 26.0f);
                    button(panel_x + 382.0f, footer_y, 96.0f, "Haut", hot_up,
                           {0.20f, 0.28f, 0.36f, 1.0f});
                    button(panel_x + 492.0f, footer_y, 96.0f, "Bas", hot_down,
                           {0.20f, 0.28f, 0.36f, 1.0f});
                    const std::string page = std::to_string(module_scroll + 1) + "-" +
                        std::to_string(list_end) + " / " + std::to_string(ordered_statuses.size());
                    fmk::draw_text(panel_x + 20.0f, footer_y + 5.0f, 14.0f,
                                   {0.65f, 0.75f, 0.82f, 1.0f}, page.c_str());
                    if (clicked && dragging.empty() && hot_up && module_scroll > 0) {
                        --module_scroll; state_dirty = true;
                    }
                    if (clicked && dragging.empty() && hot_down && module_scroll < max_scroll) {
                        ++module_scroll; state_dirty = true;
                    }
                }
            }

            if (!plugin_open["blaakan.inventory"] && atlas_input_open) {
                fmk::input_set_visible(false);
                atlas_input_open = false;
            }
            fmk::input_set_wheel_rect(0, 0, 0, 0);
            std::size_t window_index = 0;
            for (const auto& status : statuses) {
                if (!status.enabled || (status.manifest.requires_game_world && !world_available) ||
                    !plugin_open[status.manifest.id]) continue;
                auto& pos = plugin_windows[status.manifest.id];
                if (status.manifest.id == "blaakan.inventory") {
                    if (!atlas_input_open) {
                        fmk::input_set_visible(true);
                        atlas_input_open = true;
                    }
                    if (InterlockedCompareExchange(&g_atlas_ready, 0, 0))
                        fmk::atlas_ui_draw(width, height);
                    else
                        fmk::draw_text(pos.x, pos.y, 16, {1,1,1,1}, "Collection Atlas loading...");
                    fmk::InputState atlas_state{};
                    fmk::input_peek(&atlas_state);
                    if (!atlas_state.visible) {
                        plugin_open[status.manifest.id] = false;
                        atlas_input_open = false;
                        state_dirty = true;
                    }
                    ++window_index;
                    continue;
                }
                const auto rendered = g_plugins->rendered_text(status.manifest.id);
                if (rendered.empty()) continue;
                const bool bossrun_window = status.manifest.id == "patobeur.bossrun";
                const bool console_window = status.manifest.id == "patobeur.console";
                bool bossrun_has_stats = false;
                std::string detected_kind = "aucun";
                std::string detected_class;
                bool detected_is_boss = false;
                std::string timer_state = "idle";
                std::string timer_value = "00:00:00";
                std::string last_value, average_value, kills_value = "0", wipes_value = "0";
                for (const auto& line : rendered) {
                    if (line.rfind("DETECTED|", 0) == 0) {
                        const std::size_t sep = line.find('|', 9);
                        detected_kind = sep == std::string::npos ? line.substr(9)
                                                                 : line.substr(9, sep - 9);
                        if (sep != std::string::npos) {
                            const std::size_t boss_sep = line.find('|', sep + 1);
                            detected_class = boss_sep == std::string::npos
                                ? line.substr(sep + 1)
                                : line.substr(sep + 1, boss_sep - sep - 1);
                            detected_is_boss = boss_sep != std::string::npos &&
                                               line.substr(boss_sep + 1) == "boss";
                        }
                    } else if (line.rfind("TIMER|", 0) == 0) {
                        const std::size_t sep = line.find('|', 6);
                        timer_state = sep == std::string::npos ? "idle" : line.substr(6, sep - 6);
                        if (sep != std::string::npos) timer_value = line.substr(sep + 1);
                    } else if (line.rfind("STATS|", 0) == 0) {
                        const std::size_t sep = line.find('|', 6);
                        if (sep != std::string::npos) {
                            last_value = line.substr(6, sep - 6);
                            average_value = line.substr(sep + 1);
                            bossrun_has_stats = true;
                        }
                    } else if (line.rfind("COUNTS|", 0) == 0) {
                        const std::size_t sep = line.find('|', 7);
                        if (sep != std::string::npos) {
                            kills_value = line.substr(7, sep - 7);
                            wipes_value = line.substr(sep + 1);
                        }
                    }
                }
                const float plugin_width = bossrun_window ? 400.0f
                    : (console_window ? 720.0f : 470.0f);
                const float plugin_height = bossrun_window
                    ? (bossrun_has_stats ? 190.0f : 166.0f)
                    : (console_window ? 430.0f
                                      : 58.0f + (float)rendered.size() * 24.0f);
                const bool bossrun_toggle_hot = false;
                const float plugin_close_x = pos.x + plugin_width - 30.0f;
                const float plugin_close_y = pos.y + 6.0f;
                const bool plugin_close_hot = in_rect(plugin_close_x, plugin_close_y, 22.0f, 22.0f);
                if (clicked && dragging.empty() && plugin_close_hot) {
                    plugin_open[status.manifest.id] = false;
                    state_dirty = true;
                }
                if (clicked && dragging.empty() && !plugin_close_hot &&
                    in_rect(pos.x, pos.y, plugin_width, 34.0f) &&
                    !bossrun_toggle_hot)
                    start_drag(status.manifest.id, pos.x, pos.y);
                if (click_down && cursor_valid && dragging == status.manifest.id) {
                    pos.x = (float)cursor.x - drag_dx;
                    pos.y = (float)cursor.y - drag_dy;
                    clamp_window(pos.x, pos.y, plugin_width, plugin_height);
                }
                fmk::draw_rect(pos.x, pos.y, plugin_width, plugin_height,
                               {0.04f, 0.07f, 0.09f, 0.94f});
                fmk::draw_rect(pos.x, pos.y, plugin_width, 34.0f,
                               {0.10f, 0.18f, 0.23f, 0.98f});
                fmk::draw_rect_outline(pos.x, pos.y, plugin_width, plugin_height, 2.0f,
                                       {0.30f, 0.80f, 0.70f, 1.0f});
                fmk::draw_text(pos.x + 14.0f, pos.y + 8.0f, 18.0f,
                               {1.0f, 1.0f, 1.0f, 1.0f}, status.manifest.name.c_str());                if (g_close_icon_texture >= 1)
                    fmk::draw_image(g_close_icon_texture, plugin_close_x, plugin_close_y,
                                    22, 22, 0, 0, 1, 1,
                                    plugin_close_hot
                                        ? fmk::Color{1.0f,0.72f,0.72f,1.0f}
                                        : fmk::Color{1.0f,1.0f,1.0f,1.0f});
                else
                    fmk::draw_text(plugin_close_x + 5, plugin_close_y + 1, 16,
                                   {1,1,1,1}, "x");
                if (clicked && plugin_close_hot) {
                    ++window_index;
                    continue;
                }

                if (bossrun_window) {
                    const bool has_detection = detected_kind != "aucun" && !detected_kind.empty();
                    const float name_y = pos.y + 40.0f;
                    fmk::draw_rect(pos.x + 8, name_y, plugin_width - 16, 32,
                                   {0.66f, 0.68f, 0.69f, 0.96f});
                    const bool is_boss = detected_is_boss;
                    const fmk::Color name_color = !has_detection
                        ? fmk::Color{0.28f,0.30f,0.32f,1.0f}
                        : (is_boss ? fmk::Color{0.88f,0.16f,0.12f,1.0f}
                                   : fmk::Color{0.10f,0.55f,0.22f,1.0f});
                    const float name_w = fmk::measure_text(18.0f, detected_kind.c_str());
                    const float name_x = has_detection && !is_boss
                        ? pos.x + (plugin_width - name_w - 36.0f) * 0.5f
                        : pos.x + (plugin_width - name_w) * 0.5f;
                    fmk::draw_text(name_x, name_y + 7, 18.0f, name_color,
                                   detected_kind.c_str());                    if (has_detection && !is_boss) {
                        const bool tracked = g_plugins->memory().boss_tracking_enabled(detected_kind);
                        const float group_left = pos.x + (plugin_width - name_w - 36.0f) * 0.5f;
                        const float toggle_x = group_left + name_w + 8.0f;
                        const float toggle_y = name_y + 2.0f;
                        const bool toggle_hot = in_rect(toggle_x, toggle_y, 28.0f, 28.0f);
                        const int texture = tracked ? g_bossrun_enabled_texture
                                                    : g_bossrun_disabled_texture;
                        if (texture >= 1)
                            fmk::draw_image(texture, toggle_x + 2, toggle_y + 2, 24, 24,
                                            0, 0, 1, 1, {1,1,1,1});
                        fmk::draw_rect_outline(toggle_x, toggle_y, 28, 28, 1.0f,
                                               {0.25f,0.35f,0.38f,1.0f});
                        if (clicked && dragging.empty() && toggle_hot)
                            g_plugins->memory().set_boss_tracking_enabled(detected_kind, !tracked);
                    }

                    const fmk::Color timer_color = timer_state == "running"
                        ? fmk::Color{1.0f,0.48f,0.10f,1.0f}
                        : (timer_state == "finished"
                           ? fmk::Color{1.0f,0.88f,0.20f,1.0f}
                           : fmk::Color{1.0f,1.0f,1.0f,1.0f});
                    const float timer_w = fmk::measure_text(38.0f, timer_value.c_str());
                    fmk::draw_text(pos.x + (plugin_width - timer_w) * 0.5f, pos.y + 82,
                                   38.0f, timer_color, timer_value.c_str());
                    float footer_y = pos.y + 140.0f;
                    if (bossrun_has_stats) {
                        const std::string stats = "Dernier " + last_value +
                                                  "    Moyen " + average_value;
                        const float stats_w = fmk::measure_text(14.0f, stats.c_str());
                        fmk::draw_text(pos.x + (plugin_width - stats_w) * 0.5f, pos.y + 134,
                                       14.0f, {0.72f,0.78f,0.82f,1.0f}, stats.c_str());
                        footer_y = pos.y + 162.0f;
                    }
                    const std::string counts = "Victoires " + kills_value +
                                               "    Echecs " + wipes_value;
                    const float counts_w = fmk::measure_text(13.0f, counts.c_str());
                    fmk::draw_text(pos.x + (plugin_width - counts_w) * 0.5f, footer_y,
                                   13.0f, {0.55f,0.65f,0.70f,1.0f}, counts.c_str());
                } else if (console_window) {
                    constexpr std::size_t visible_lines = 17;
                    static std::size_t console_back = 0;
                    static std::size_t console_last_count = 0;
                    if (rendered.size() != console_last_count && console_back == 0)
                        console_last_count = rendered.size();
                    const int wheel = fmk::input_take_wheel_in((int)pos.x, (int)(pos.y + 34),
                                                               (int)plugin_width,
                                                               (int)(plugin_height - 34));
                    fmk::input_set_wheel_rect((int)pos.x, (int)(pos.y + 34),
                                              (int)plugin_width,
                                              (int)(plugin_height - 34));
                    const std::size_t max_back = rendered.size() > visible_lines
                        ? rendered.size() - visible_lines : 0;
                    if (wheel > 0) console_back = std::min(max_back, console_back + (std::size_t)wheel);
                    if (wheel < 0) {
                        const std::size_t down = (std::size_t)(-wheel);
                        console_back = down >= console_back ? 0 : console_back - down;
                    }
                    console_back = std::min(console_back, max_back);
                    const std::size_t end_line = rendered.size() - console_back;
                    const std::size_t first_line = end_line > visible_lines
                        ? end_line - visible_lines : 0;
                    fmk::draw_rect(pos.x + 8, pos.y + 40, plugin_width - 16,
                                   plugin_height - 50,
                                   {0.015f,0.025f,0.032f,0.98f});
                    float line_y = pos.y + 48.0f;
                    for (std::size_t i = first_line; i < end_line; ++i) {
                        const std::string line = rendered[i].rfind("LOG|", 0) == 0
                            ? rendered[i].substr(4) : rendered[i];
                        fmk::draw_text(pos.x + 16, line_y, 15.0f,
                                       {0.62f,0.88f,0.72f,1.0f}, line.c_str());
                        line_y += 21.0f;
                    }
                    const std::string scroll_info = std::to_string(first_line + (rendered.empty()?0:1)) +
                        "-" + std::to_string(end_line) + " / " + std::to_string(rendered.size());
                    fmk::draw_text(pos.x + plugin_width - 112, pos.y + 10, 13.0f,
                                   {0.68f,0.76f,0.82f,1.0f}, scroll_info.c_str());
                } else {
                    float text_y = pos.y + 42.0f;
                    for (const auto& line : rendered) {
                        fmk::draw_text(pos.x + 14.0f, text_y, 18.0f,
                                       {0.45f, 0.90f, 0.65f, 1.0f}, line.c_str());
                        text_y += 24.0f;
                    }
                }
                ++window_index;
            }
            (void)window_index;
            if (!icon_tooltip.empty() && cursor_valid && dragging.empty()) {
                constexpr float tip_font = 15.0f;
                const float tip_w = fmk::measure_text(tip_font, icon_tooltip.c_str()) + 20.0f;
                const float tip_h = 30.0f;
                float tip_x = (float)cursor.x + 18.0f;
                float tip_y = (float)cursor.y + 18.0f;
                tip_x = std::clamp(tip_x, 0.0f, std::max(0.0f, width - tip_w));
                tip_y = std::clamp(tip_y, 0.0f, std::max(0.0f, height - tip_h));
                fmk::draw_rect(tip_x, tip_y, tip_w, tip_h,
                               {0.025f, 0.045f, 0.060f, 0.98f});
                fmk::draw_rect_outline(tip_x, tip_y, tip_w, tip_h, 1.0f,
                                       {0.35f, 0.85f, 0.95f, 1.0f});
                fmk::draw_text(tip_x + 10.0f, tip_y + 7.0f, tip_font,
                               {0.92f, 0.96f, 0.98f, 1.0f}, icon_tooltip.c_str());
            }
            if (released && !dragging.empty()) state_dirty = true;
            if (state_dirty) save_state();
            if (released) dragging.clear();
            previous_click = click_down;
        });
    }
    if (!fmk::overlay_install_stage(native.overlay_stage)) log_line("overlay installation failed");
    else {
        log_line("overlay stage %d installed", native.overlay_stage);
        HANDLE atlas_thread = CreateThread(nullptr, 0, atlas_worker, nullptr, 0, nullptr);
        if (atlas_thread) CloseHandle(atlas_thread);
    }
    return 0;
}
} // namespace

namespace fmk {
void host_log(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
    va_end(args);
    log_line("%s", buffer);
}
} // namespace fmk

extern "C" HRESULT WINAPI Proxy_CreateDXGIFactory(REFIID riid, void** output) {
    using Fn = HRESULT(WINAPI*)(REFIID, void**);
    return create_factory("CreateDXGIFactory", forward_export<Fn>("CreateDXGIFactory"), riid, output);
}

extern "C" HRESULT WINAPI Proxy_CreateDXGIFactory1(REFIID riid, void** output) {
    using Fn = HRESULT(WINAPI*)(REFIID, void**);
    return create_factory("CreateDXGIFactory1", forward_export<Fn>("CreateDXGIFactory1"), riid, output);
}

extern "C" HRESULT WINAPI Proxy_CreateDXGIFactory2(UINT flags, REFIID riid, void** output) {
    using Fn = HRESULT(WINAPI*)(UINT, REFIID, void**);
    const auto fn = forward_export<Fn>("CreateDXGIFactory2");
    if (!fn) return E_FAIL;
    const HRESULT hr = fn(flags, riid, output);
    if (SUCCEEDED(hr) && output && *output) *output = fmk::dxgi_wrap_factory(*output, riid);
    return hr;
}

extern "C" HRESULT WINAPI Proxy_DXGIGetDebugInterface1(UINT flags, REFIID riid, void** output) {
    using Fn = HRESULT(WINAPI*)(UINT, REFIID, void**);
    const auto fn = forward_export<Fn>("DXGIGetDebugInterface1");
    return fn ? fn(flags, riid, output) : E_NOINTERFACE;
}

extern "C" HRESULT WINAPI Proxy_DXGIDeclareAdapterRemovalSupport() {
    using Fn = HRESULT(WINAPI*)();
    const auto fn = forward_export<Fn>("DXGIDeclareAdapterRemovalSupport");
    return fn ? fn() : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);
        InitializeCriticalSection(&g_log_lock);
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(instance, path, MAX_PATH);
        std::filesystem::path dll_path(path);
        const auto game_root = dll_path.parent_path();
        const auto kit_root = game_root / L"farevermodkit";
        const auto log_root = kit_root / L"logs";
        std::error_code log_error;
        std::filesystem::create_directories(log_root, log_error);
        const auto log_path = (log_root / L"farevermodkit-native.log").wstring();
        g_log = _wfsopen(log_path.c_str(), L"a", _SH_DENYNO);
        fmk::set_memory_log_sink(memory_log_sink);
        log_line("proxy loaded");
        HANDLE worker = CreateThread(nullptr, 0, overlay_worker, nullptr, 0, nullptr);
        if (worker) CloseHandle(worker);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_plugins) {
            g_plugins->shutdown();
            g_plugins.reset();
        }
        fmk::overlay_shutdown();
        if (g_log) fclose(g_log);
        DeleteCriticalSection(&g_log_lock);
        if (g_real) FreeLibrary(g_real);
    }
    return TRUE;
}