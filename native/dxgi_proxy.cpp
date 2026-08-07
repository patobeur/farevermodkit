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
#include "window_frame.h"
#include "paths.h"
#include "report.h"
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
int g_down_texture = -1;
int g_up_texture = -1;
int g_resize_icon_texture = -1;
int g_reset_icons_texture = -1;
int g_reset_windows_texture = -1;
int g_lock_texture = -1;
int g_unlock_texture = -1;
int g_module_on_texture = -1;
int g_module_off_texture = -1;
int g_module_options_texture = -1;
int g_bossrun_enabled_texture = -1;
int g_bossrun_disabled_texture = -1;
std::unordered_map<std::string, int> g_author_icon_textures;
std::unordered_map<std::string, int> g_module_icon_textures;
std::unordered_map<std::string, std::unordered_map<std::string, int>> g_module_asset_textures;

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
    const auto resize_icon = find_upward("farevermodkit/assets/fmk/resize.png");
    const auto reset_icons = find_upward("farevermodkit/assets/fmk/reset-icons.png");
    const auto reset_windows = find_upward("farevermodkit/assets/fmk/reset-windows.png");
    const auto lock_icon = find_upward("farevermodkit/assets/fmk/lock.png");
    const auto unlock_icon = find_upward("farevermodkit/assets/fmk/unlock.png");
    if (!resize_icon.empty()) {
        g_resize_icon_texture = fmk::overlay_load_image(resize_icon.u8string().c_str());
        fmk::atlas_ui_set_resize_texture(g_resize_icon_texture);
    }
    if (!reset_icons.empty()) g_reset_icons_texture = fmk::overlay_load_image(reset_icons.u8string().c_str());
    if (!reset_windows.empty()) g_reset_windows_texture = fmk::overlay_load_image(reset_windows.u8string().c_str());
    if (!lock_icon.empty()) g_lock_texture = fmk::overlay_load_image(lock_icon.u8string().c_str());
    if (!unlock_icon.empty()) g_unlock_texture = fmk::overlay_load_image(unlock_icon.u8string().c_str());
    const auto close_icon = find_upward("farevermodkit/assets/fmk/close.png");
    if (!close_icon.empty()) {
        g_close_icon_texture = fmk::overlay_load_image(close_icon.u8string().c_str());
        fmk::atlas_ui_set_close_texture(g_close_icon_texture);
    }
    const auto down_icon = find_upward("farevermodkit/assets/fmk/down.png");
    if (!down_icon.empty()) g_down_texture = fmk::overlay_load_image(down_icon.u8string().c_str());
    const auto up_icon = find_upward("farevermodkit/assets/fmk/up.png");
    if (!up_icon.empty()) g_up_texture = fmk::overlay_load_image(up_icon.u8string().c_str());
    const auto on_icon = find_upward("farevermodkit/assets/fmk/on.png");
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
            ? fmk::overlay_load_image(path.u8string().c_str()) : -1;        const auto assets = status.manifest.directory / "assets";
        std::error_code asset_error;
        std::size_t asset_count = 0;
        if (std::filesystem::is_directory(assets, asset_error)) {
            for (std::filesystem::recursive_directory_iterator it(assets, asset_error), end;
                 it != end && !asset_error && asset_count < 64; it.increment(asset_error)) {
                if (!it->is_regular_file() || it->path().extension() != ".png") continue;
                auto key = std::filesystem::relative(it->path(), assets, asset_error).generic_u8string();
                if (asset_error || key.empty()) { asset_error.clear(); continue; }
                auto& module_textures = g_module_asset_textures[status.manifest.id];
                // Asset discovery runs from the Present callback. Never upload
                // the same PNG again on the next frame: repeated uploads used
                // descriptor slots until the heap was exhausted and could
                // corrupt the DX12 present path.
                if (module_textures.find(key) == module_textures.end()) {
                    const int texture = fmk::overlay_load_image(it->path().u8string().c_str());
                    if (texture >= 0) module_textures[key] = texture;
                }
                if (module_textures.find(key) != module_textures.end()) ++asset_count;
            }
        }
        if (status.manifest.id == "blaakan.atlas")
            fmk::atlas_ui_set_title_texture(g_module_icon_textures[status.manifest.id]);
        if (status.manifest.id == "patobeur.report") {
            fmk::report_set_module_dir(status.manifest.directory);
        }
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
    DWORD last_map_entities_update = 0;
    std::vector<fmk::NearbyEntity> cached_map_entities;
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
            fmk::MapSnapshot map{};
            map.player_valid = pose;
            map.player_x = x; map.player_y = y; map.player_z = z;
            map.player_rotation = rot;
            map.camera_valid = camera;
            map.camera_x = px; map.camera_y = py; map.camera_z = pz;
            map.camera_target_x = tx; map.camera_target_y = ty; map.camera_target_z = tz;
            const DWORD map_now = GetTickCount();
            if (map_now - last_map_entities_update >= 250) {
                last_map_entities_update = map_now;
                g_plugins->memory().read_nearby_entities(250.0, &cached_map_entities);
            }
            map.entities = cached_map_entities;
            g_plugins->memory().read_world_name(&map.world_name);
            g_plugins->memory().update_map_snapshot(map);
        } else {
            fmk::nav_set_hero_pose(false,0,0,0,0);
            fmk::nav_set_camera(false,0,0,0,0,0,0);
            cached_map_entities.clear();
            g_plugins->memory().update_map_snapshot({});
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
            const bool atlas_save_requested = fmk::atlas_ui_take_save_request();
            const bool report_requested =
                g_plugins->memory().take_report_export_request();
            const bool save_requested = atlas_save_requested || report_requested;
            fmk::atlas_ui_update(collection, inventories, units, jobs, runes,
                                 completion, mastery, save_requested);
            if (report_requested) fmk::report_refresh();
            if (atlas_save_requested) fmk::atlas_ui_mark_saved();
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
            struct WindowState { float x, y, w = 0.0f, h = 0.0f; };
            struct IconState { float x, y; bool moved; };
            static bool manager_open = false;
            static bool previous_click = false;
            static bool capture_mode = false;
            static bool previous_f2 = false;
            static float panel_x = 0.0f;
            static float panel_y = 146.0f;
            static float panel_width = 650.0f;
            static float panel_height_saved = 0.0f;
            static IconState fmk_icon{0.0f, 146.0f, false};
            static bool layout_initialized = false;
            static std::size_t module_scroll = 0;
            static std::unordered_map<std::string, WindowState> plugin_windows;
            static WindowState bossrun_history_window{0.0f, 0.0f};
            static bool bossrun_history_initialized = false;
            static bool bossrun_history_moved = false;
            static bool bossrun_history_open = true;
            static std::unordered_map<std::string, IconState> plugin_icons;
            static std::unordered_map<std::string, bool> plugin_open;
            static std::string dragging;
            static float drag_dx = 0.0f;
            static float drag_dy = 0.0f;
            static float resize_origin_w = 0.0f;
            static float resize_origin_h = 0.0f;
            static bool state_loaded = false;
            static std::filesystem::path state_path;
            static std::unordered_map<std::string, bool> saved_enabled;
            static bool atlas_input_open = false;
            static bool reset_icons_requested = false;
            static bool reset_windows_requested = false;
            static bool icon_positions_locked = false;
            bool state_dirty = false;

            if (!state_loaded) {
                state_path = std::filesystem::path(fmk::user_data_dir()) /
                             "settings" / "ui-state.json";
                const auto native_config = find_upward("farevermodkit/config/native.json");
                const auto legacy_state = native_config.empty()
                    ? std::filesystem::path{}
                    : native_config.parent_path() / "ui-state.json";
                if (!std::filesystem::is_regular_file(state_path) &&
                    std::filesystem::is_regular_file(legacy_state)) {
                    std::error_code migration_error;
                    std::filesystem::copy_file(legacy_state, state_path,
                        std::filesystem::copy_options::skip_existing, migration_error);
                    log_line("ui-state: legacy settings migration %s",
                             migration_error ? "failed" : "completed");
                }
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
                    panel_width = read_number("panelW", panel_width);
                    panel_height_saved = read_number("panelH", panel_height_saved);
                    fmk_icon.x = read_number("fmkX", fmk_icon.x);
                    fmk_icon.y = read_number("fmkY", fmk_icon.y);
                    fmk_icon.moved = read_bool("fmkMoved", true);
                    icon_positions_locked = read_bool("iconPositionsLocked", false);
                    bossrun_history_window.x = read_number("bossRunHistoryX", 0.0f);
                    bossrun_history_window.y = read_number("bossRunHistoryY", 0.0f);
                    bossrun_history_window.w = read_number("bossRunHistoryW", 400.0f);
                    bossrun_history_window.h = read_number("bossRunHistoryH", 110.0f);
                    bossrun_history_moved = read_bool("bossRunHistoryMoved", false);
                    bossrun_history_initialized = bossrun_history_moved;
                    bossrun_history_open = read_bool("bossRunHistoryOpen", true);
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
                    const std::regex size_pattern(
                        "\\{\\s*\\\"id\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"\\s*,\\s*"
                        "\\\"w\\\"\\s*:\\s*(-?[0-9.]+)\\s*,\\s*"
                        "\\\"h\\\"\\s*:\\s*(-?[0-9.]+)\\s*\\}");
                    for (std::sregex_iterator it(saved.begin(), saved.end(), size_pattern), end;
                         it != end; ++it) {
                        auto& win = plugin_windows[(*it)[1].str()];
                        win.w = std::stof((*it)[2].str());
                        win.h = std::stof((*it)[3].str());
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

            // Screenshot mode observes F2 without consuming it: the game still
            // receives its own F2 action. Only FMK rendering/input is suspended.
            const bool f2_down = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
            if (f2_down && !previous_f2) capture_mode = !capture_mode;
            previous_f2 = f2_down;
            if (capture_mode) {
                if (atlas_input_open) {
                    fmk::input_set_visible(false);
                    atlas_input_open = false;
                }
                if (!manager_open) fmk::input_set_wheel_rect(0, 0, 0, 0);
                dragging.clear();
                previous_click = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
                return;
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
            auto start_resize = [&](const std::string& id, float w, float h) {
                dragging = "%" + id;
                drag_dx = (float)cursor.x;
                drag_dy = (float)cursor.y;
                resize_origin_w = w;
                resize_origin_h = h;
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
                       << "  \"panelW\": " << panel_width << ",\n"
                       << "  \"panelH\": " << panel_height_saved << ",\n"
                       << "  \"fmkX\": " << fmk_icon.x << ",\n"
                       << "  \"fmkY\": " << fmk_icon.y << ",\n"
                       << "  \"fmkMoved\": " << (fmk_icon.moved ? "true" : "false") << ",\n"
                       << "  \"iconPositionsLocked\": " << (icon_positions_locked ? "true" : "false") << ",\n"
                       << "  \"bossRunHistoryX\": " << bossrun_history_window.x << ",\n"
                       << "  \"bossRunHistoryY\": " << bossrun_history_window.y << ",\n"
                       << "  \"bossRunHistoryW\": " << bossrun_history_window.w << ",\n"
                       << "  \"bossRunHistoryH\": " << bossrun_history_window.h << ",\n"
                       << "  \"bossRunHistoryMoved\": " << (bossrun_history_moved ? "true" : "false") << ",\n"
                       << "  \"bossRunHistoryOpen\": " << (bossrun_history_open ? "true" : "false") << ",\n"
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
                output << "\n  ],\n  \"windowSizes\": [\n";
                first = true;
                for (const auto& status : current_statuses) {
                    if (!first) output << ",\n";
                    first = false;
                    const auto it = plugin_windows.find(status.manifest.id);
                    const WindowState win = it != plugin_windows.end()
                        ? it->second : WindowState{};
                    output << "    {\"id\":\"" << status.manifest.id
                           << "\",\"w\":" << win.w << ",\"h\":" << win.h << "}";
                }
                output << "\n  ]\n}\n";
            };
            if (reset_icons_requested && width > panel_width + 90.0f) {
                fmk_icon = {width - 76.0f,
                            std::clamp(146.0f, 0.0f, std::max(0.0f, height - 58.0f)),
                            false};
                plugin_icons.clear();
                reset_icons_requested = false;
                state_dirty = true;
            }
            if (reset_windows_requested) {
                panel_width = 650.0f;
                panel_height_saved = 0.0f;
                panel_x = std::max(0.0f, width - 76.0f - panel_width - 8.0f);
                panel_y = std::clamp(146.0f, 0.0f, std::max(0.0f, height - 120.0f));
                plugin_windows.clear();
                bossrun_history_window = {};
                bossrun_history_moved = false;
                bossrun_history_initialized = false;
                fmk::atlas_ui_reset_layout();
                reset_windows_requested = false;
                state_dirty = true;
            }
            if (!layout_initialized && width > panel_width + 90.0f) {
                fmk_icon.x = width - 76.0f;
                fmk_icon.y = std::clamp(146.0f, 0.0f, std::max(0.0f, height - 58.0f));
                panel_x = std::max(0.0f, fmk_icon.x - panel_width - 8.0f);
                panel_y = fmk_icon.y;
                layout_initialized = true;
            }
            std::size_t enabled_index = 0;
            for (const auto& status : statuses) {
                if (!status.enabled) {
                    plugin_open[status.manifest.id] = false;
                    continue;
                }
                // A world-dependent window is only hidden during loading. Keep
                // the player's open/closed choice so it returns in the next world.
                if (status.manifest.requires_game_world && !world_available) continue;
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
                if (icon_positions_locked) {
                    manager_open = !manager_open;
                    state_dirty = true;
                } else {
                    fmk_icon.moved = false;
                    start_drag("@fmk", fmk_icon.x, fmk_icon.y);
                }
            }
            if (clicked && dragging.empty()) {
                for (const auto& status : statuses) {
                    if (!status.enabled || (status.manifest.requires_game_world && !world_available)) continue;
                    auto& icon = plugin_icons[status.manifest.id];
                    if (in_rect(icon.x, icon.y, icon_size, icon_size)) {
                        if (status.manifest.id == "patobeur.bossrun") {
                            const bool desired = !(plugin_open[status.manifest.id] ||
                                                   bossrun_history_open);
                            plugin_open[status.manifest.id] = desired;
                            bossrun_history_open = desired;
                        } else {
                            plugin_open[status.manifest.id] =
                                !plugin_open[status.manifest.id];
                        }
                        state_dirty = true;
                        log_line("ui: %s open=%d", status.manifest.id.c_str(),
                                 plugin_open[status.manifest.id] ? 1 : 0);
                        if (!icon_positions_locked) {
                            icon.moved = false;
                            start_drag("@" + status.manifest.id, icon.x, icon.y);
                        }
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
                std::string label = status.manifest.id == "blaakan.atlas"
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
            if (panel_height_saved <= 0.0f)
                panel_height_saved = std::min(650.0f, height - panel_y);
            const float panel_height = std::max(180.0f, panel_height_saved);
            const std::size_t max_scroll = ordered_statuses.empty()
                ? 0 : ordered_statuses.size() - 1;
            module_scroll = std::min(module_scroll, max_scroll);
            if (manager_open) {
                const int wheel = fmk::input_take_wheel_in(
                    (int)panel_x, (int)(panel_y + 34.0f),
                    (int)panel_width, (int)(panel_height - 34.0f));
                if (wheel > 0 && module_scroll > 0) {
                    const std::size_t amount = (std::size_t)wheel;
                    module_scroll = amount >= module_scroll ? 0 : module_scroll - amount;
                    state_dirty = true;
                } else if (wheel < 0 && module_scroll < max_scroll) {
                    module_scroll = std::min(max_scroll,
                        module_scroll + (std::size_t)(-wheel));
                    state_dirty = true;
                }
                fmk::input_set_wheel_rect((int)panel_x, (int)(panel_y + 34.0f),
                                          (int)panel_width,
                                          (int)(panel_height - 34.0f));
            }
            std::size_t list_end = module_scroll;
            float list_used = 0.0f;
            const float list_capacity = std::max(38.0f, panel_height - 80.0f);
            while (list_end < ordered_statuses.size()) {
                const float needed = row_height;
                if (list_end > module_scroll && list_used + needed > list_capacity) break;
                list_used += needed;
                ++list_end;
            }
            const std::size_t visible_modules = list_end - module_scroll;
            const bool can_scroll = module_scroll > 0 || list_end < ordered_statuses.size();
            if (manager_open) {
                const float manager_close_x = panel_x + panel_width - 30.0f;
                const float manager_close_y = panel_y + 6.0f;
                const bool manager_close_hot = in_rect(manager_close_x, manager_close_y, 22.0f, 22.0f);
                const float manager_down_x = manager_close_x - 30.0f;
                const float manager_up_x = manager_down_x - 30.0f;
                const bool manager_down_hot = in_rect(manager_down_x, manager_close_y, 22.0f, 22.0f);
                const bool manager_up_hot = in_rect(manager_up_x, manager_close_y, 22.0f, 22.0f);
                const float reset_icons_x = panel_x + 224.0f;
                const float reset_windows_x = panel_x + 258.0f;
                const float icon_lock_x = panel_x + 292.0f;
                const bool reset_icons_hot = in_rect(reset_icons_x, panel_y + 4.0f, 26.0f, 26.0f);
                const bool reset_windows_hot = in_rect(reset_windows_x, panel_y + 4.0f, 26.0f, 26.0f);
                const bool icon_lock_hot = in_rect(icon_lock_x, panel_y + 4.0f, 26.0f, 26.0f);
                const bool manager_resize_hot = in_rect(panel_x + panel_width - 20.0f,
                                                        panel_y + panel_height - 20.0f,
                                                        20.0f, 20.0f);
                if (clicked && dragging.empty() && manager_close_hot) {
                    manager_open = false;
                    state_dirty = true;
                } else if (clicked && dragging.empty() && manager_down_hot) {
                    if (module_scroll < max_scroll) {
                        module_scroll = std::min(max_scroll, module_scroll + 1);
                        state_dirty = true;
                    }
                } else if (clicked && dragging.empty() && manager_up_hot) {
                    if (module_scroll > 0) {
                        module_scroll -= 1;
                        state_dirty = true;
                    }
                } else if (clicked && dragging.empty() && reset_icons_hot) {
                    reset_icons_requested = true;
                } else if (clicked && dragging.empty() && reset_windows_hot) {
                    reset_windows_requested = true;
                } else if (clicked && dragging.empty() && icon_lock_hot) {
                    icon_positions_locked = !icon_positions_locked;
                    state_dirty = true;
                } else if (clicked && dragging.empty() && manager_resize_hot) {
                    start_resize("manager", panel_width, panel_height);
                }
                if (clicked && dragging.empty() && !manager_close_hot &&
                    !manager_down_hot && !manager_up_hot &&
                    !reset_icons_hot && !reset_windows_hot && !icon_lock_hot &&
                    !manager_resize_hot &&
                    in_rect(panel_x, panel_y, panel_width, 34.0f))
                    start_drag("$manager", panel_x, panel_y);                if (click_down && cursor_valid && dragging == "%manager") {
                    panel_width = std::clamp(resize_origin_w + (float)cursor.x - drag_dx,
                                             560.0f, width - panel_x);
                    panel_height_saved = std::clamp(
                        resize_origin_h + (float)cursor.y - drag_dy,
                        180.0f, height - panel_y);
                }
                if (click_down && cursor_valid && dragging == "$manager") {
                    panel_x = (float)cursor.x - drag_dx;
                    panel_y = (float)cursor.y - drag_dy;
                    clamp_window(panel_x, panel_y, panel_width, panel_height);
                }
                fmk::draw_window_frame(
                    {panel_x, panel_y, panel_width, panel_height,
                     "FareverModKit", g_fmk_icon_texture, g_close_icon_texture,
                     34.0f, {0.05f,0.08f,0.10f,0.94f},
                     {0.08f,0.13f,0.17f,0.98f}, {0.25f,0.75f,0.95f,1.0f},
                     true, g_resize_icon_texture},
                    {cursor_valid, (float)cursor.x, (float)cursor.y, clicked});
                if (g_down_texture >= 1)
                    fmk::draw_image(g_down_texture, manager_down_x, manager_close_y, 22, 22, 0, 0, 1, 1, manager_down_hot ? fmk::Color{1,1,0.75f,1} : fmk::Color{1,1,1,1});
                if (g_up_texture >= 1)
                    fmk::draw_image(g_up_texture, manager_up_x, manager_close_y, 22, 22, 0, 0, 1, 1, manager_up_hot ? fmk::Color{1,1,0.75f,1} : fmk::Color{1,1,1,1});
                if (g_reset_icons_texture >= 1)
                    fmk::draw_image(g_reset_icons_texture, reset_icons_x, panel_y + 4.0f,
                                    26, 26, 0, 0, 1, 1,
                                    reset_icons_hot ? fmk::Color{1,1,0.75f,1}
                                                    : fmk::Color{1,1,1,1});
                if (g_reset_windows_texture >= 1)
                    fmk::draw_image(g_reset_windows_texture, reset_windows_x, panel_y + 4.0f,
                                    26, 26, 0, 0, 1, 1,
                                    reset_windows_hot ? fmk::Color{1,1,0.75f,1}
                                                      : fmk::Color{1,1,1,1});
                const int icon_lock_texture = icon_positions_locked
                    ? g_lock_texture : g_unlock_texture;
                if (icon_lock_texture >= 1)
                    fmk::draw_image(icon_lock_texture, icon_lock_x, panel_y + 4.0f,
                                    26, 26, 0, 0, 1, 1,
                                    icon_lock_hot ? fmk::Color{1,1,0.75f,1}
                                                  : fmk::Color{1,1,1,1});
                if (reset_icons_hot)
                    icon_tooltip = "Reinitialiser la position des icones";
                else if (reset_windows_hot)
                    icon_tooltip = "Reinitialiser les fenetres";
                else if (icon_lock_hot)
                    icon_tooltip = icon_positions_locked
                        ? "Deverrouiller la position des icones"
                        : "Verrouiller la position des icones";
                fmk::draw_text(panel_x + 20.0f, panel_y + 42.0f, 15.0f,
                               {0.65f, 0.75f, 0.82f, 1.0f},
                               "Activer un mod affiche son icone");
                float y = panel_y + 70.0f;
                for (std::size_t module_index = module_scroll;
                     module_index < list_end; ++module_index) {
                    const auto& status = *ordered_statuses[module_index];
                    
                    bool all_deps_met = true;
                    std::string missing_deps;
                    for (const auto& dep : status.manifest.dependencies) {
                        bool found = false;
                        for (const auto& other : statuses) {
                            if (other.manifest.id == dep && other.enabled) { found = true; break; }
                        }
                        if (dep == "fmk.atlas") found = true; // Special native integration
                        if (!found) {
                            all_deps_met = false;
                            if (!missing_deps.empty()) missing_deps += ", ";
                            missing_deps += dep;
                        }
                    }

                    const float options_x = panel_x + panel_width - 38.0f;
                    const float toggle_x = options_x - 42.0f;
                    const bool hot_toggle = in_rect(toggle_x, y - 6.0f, 28.0f, 28.0f);
                    const bool hot_options = in_rect(options_x, y - 6.0f, 28.0f, 28.0f);
                    
                    const char* state = status.enabled ? "active" : "desactive";
                    const std::string line = status.manifest.name + " (" + status.manifest.author + ")";
                    fmk::Color color = status.loaded && status.enabled
                        ? fmk::Color{0.45f, 0.90f, 0.65f, 1.0f}
                        : fmk::Color{0.95f, 0.55f, 0.45f, 1.0f};
                    if (!all_deps_met) color = fmk::Color{0.95f, 0.20f, 0.20f, 1.0f}; // Red warning

                    const auto module_icon = g_module_icon_textures.find(status.manifest.id);
                    if (module_icon != g_module_icon_textures.end() && module_icon->second >= 1)
                        fmk::draw_image(module_icon->second, panel_x + 18, y - 5,
                                        26, 26, 0, 0, 1, 1, {1,1,1,1});
                    
                    fmk::draw_text(panel_x + 54.0f, y, 16.0f, color, line.c_str());

                    int toggle_texture = -1;
                    if (!all_deps_met) toggle_texture = g_module_off_texture;
                    else toggle_texture = status.enabled ? g_module_on_texture : g_module_off_texture;

                    if (toggle_texture >= 1) {
                        fmk::draw_image(toggle_texture, toggle_x, y - 6.0f, 28, 28,
                                        0, 0, 1, 1,
                                        hot_toggle && all_deps_met ? fmk::Color{1.0f,1.0f,0.78f,1.0f}
                                                                   : (all_deps_met ? fmk::Color{1,1,1,1} : fmk::Color{1,0.5f,0.5f,1}));
                    } else {
                        button(toggle_x, y - 5.0f, 28.0f, status.enabled && all_deps_met ? "-" : "+",
                               hot_toggle, {0.20f, 0.35f, 0.30f, 1.0f});
                    }

                    if (g_module_options_texture >= 1) {
                        fmk::draw_image(g_module_options_texture, options_x, y - 6.0f, 28, 28,
                                        0, 0, 1, 1,
                                        hot_options ? fmk::Color{1.0f,1.0f,0.78f,1.0f}
                                                    : fmk::Color{1,1,1,1});
                    } else {
                        button(options_x, y - 5.0f, 28.0f, "?", hot_options,
                               {0.20f, 0.28f, 0.36f, 1.0f});
                    }

                    if (hot_toggle) {
                        if (!all_deps_met) icon_tooltip = "Dependances manquantes: " + missing_deps;
                        else icon_tooltip = status.enabled ? "Desactiver " + status.manifest.name
                                                           : "Activer " + status.manifest.name;
                    } else if (hot_options) {
                        icon_tooltip = "Options de " + status.manifest.name;
                    } else if (in_rect(panel_x + 18, y - 5, 200.0f, 26.0f)) {
                        icon_tooltip = "Auteur: " + status.manifest.author + (!missing_deps.empty() ? (" | Manque: " + missing_deps) : "");
                    }

                    if (clicked && dragging.empty() && hot_toggle) {
                        if (all_deps_met) {
                            const bool desired = !status.enabled;
                            const bool changed = g_plugins->set_enabled(status.manifest.id, desired);
                            plugin_open[status.manifest.id] = desired &&
                                status.manifest.id == "patobeur.bossrun";
                            if (status.manifest.id == "patobeur.bossrun")
                                bossrun_history_open = desired;
                            state_dirty = true;
                            log_line("ui: module %s %s (%s)", status.manifest.id.c_str(),
                                     desired ? "activation" : "desactivation",
                                     changed ? "ok" : "failed");
                        } else {
                            log_line("ui: module %s cannot be enabled due to missing dependencies", status.manifest.id.c_str());
                        }
                    } else if (clicked && dragging.empty() && hot_options) {
                        log_line("ui: options requested for %s (not implemented)",
                                 status.manifest.id.c_str());
                    }
                    y += row_height;
                }
                if (can_scroll && !ordered_statuses.empty()) {
                    const float track_x = panel_x + panel_width - 12.0f;
                    const float track_y = panel_y + 70.0f;
                    const float track_h = std::max(30.0f, panel_height - 80.0f);
                    const float visible_ratio = std::min(1.0f,
                        (float)visible_modules / (float)ordered_statuses.size());
                    const float thumb_h = std::max(30.0f, track_h * visible_ratio);
                    const float scroll_ratio = max_scroll > 0
                        ? (float)module_scroll / (float)max_scroll : 0.0f;
                    float thumb_y = track_y + (track_h - thumb_h) * scroll_ratio;
                    const bool thumb_hot = in_rect(track_x, thumb_y, 8.0f, thumb_h);
                    const bool track_hot = in_rect(track_x, track_y, 8.0f, track_h);
                    if (clicked && dragging.empty() && thumb_hot) {
                        dragging = "$manager-scroll";
                        drag_dy = (float)cursor.y - thumb_y;
                    } else if (clicked && dragging.empty() && track_hot) {
                        const float ratio = std::clamp(
                            ((float)cursor.y - track_y - thumb_h * 0.5f) /
                                std::max(1.0f, track_h - thumb_h), 0.0f, 1.0f);
                        module_scroll = (std::size_t)std::lround(ratio * max_scroll);
                        state_dirty = true;
                    }
                    if (click_down && cursor_valid && dragging == "$manager-scroll") {
                        const float ratio = std::clamp(
                            ((float)cursor.y - drag_dy - track_y) /
                                std::max(1.0f, track_h - thumb_h), 0.0f, 1.0f);
                        module_scroll = (std::size_t)std::lround(ratio * max_scroll);
                    }
                    const float live_ratio = max_scroll > 0
                        ? (float)module_scroll / (float)max_scroll : 0.0f;
                    thumb_y = track_y + (track_h - thumb_h) * live_ratio;
                    fmk::draw_rect(track_x, track_y, 8.0f, track_h,
                                   {0.08f,0.12f,0.15f,0.95f});
                    fmk::draw_rect(track_x, thumb_y, 8.0f, thumb_h,
                                   thumb_hot ? fmk::Color{0.40f,0.78f,0.88f,1.0f}
                                             : fmk::Color{0.24f,0.50f,0.60f,1.0f});
                }
            }
            if ((!world_available || !plugin_open["blaakan.atlas"]) && atlas_input_open) {
                fmk::input_set_visible(false);
                atlas_input_open = false;
            }
            if (!manager_open) fmk::input_set_wheel_rect(0, 0, 0, 0);
            std::size_t window_index = 0;
            for (const auto& status : statuses) {
                if (!status.enabled || (status.manifest.requires_game_world && !world_available) ||
                    !plugin_open[status.manifest.id]) continue;
                auto& pos = plugin_windows[status.manifest.id];
                if (status.manifest.id == "blaakan.atlas") {
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
                const auto pending_draws = g_plugins->draw_commands(status.manifest.id);
                if (rendered.empty() && pending_draws.empty()) continue;
                const bool bossrun_window = status.manifest.id == "patobeur.bossrun";
                const bool console_window = status.manifest.id == "patobeur.console";
                const bool report_window = status.manifest.id == "patobeur.report";
                const bool map_window = status.manifest.id == "patobeur.map";
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
                const float default_width = bossrun_window ? 400.0f
                    : (console_window ? 720.0f : 470.0f);
                const float default_height = bossrun_window
                    ? (bossrun_has_stats ? 190.0f : 166.0f)
                    : (map_window ? 430.0f
                    : (report_window ? 168.0f
                    : (console_window ? 430.0f
                                      : 58.0f + (float)rendered.size() * 24.0f)));
                if (pos.w <= 0.0f) pos.w = default_width;
                if (pos.h <= 0.0f) pos.h = default_height;
                const float min_width = bossrun_window ? 320.0f
                    : (map_window ? 360.0f
                    : (report_window ? 390.0f
                    : (console_window ? 420.0f : 280.0f)));
                const float min_height = bossrun_window ? 166.0f
                    : (map_window ? 320.0f
                    : (report_window ? 150.0f
                    : (console_window ? 180.0f : 90.0f)));
                pos.w = std::clamp(pos.w, min_width, width - pos.x);
                pos.h = std::clamp(pos.h, min_height, height - pos.y);
                const float plugin_width = pos.w;
                const float plugin_height = pos.h;
                const bool bossrun_toggle_hot = false;
                const float plugin_close_x = pos.x + plugin_width - 30.0f;
                const float plugin_close_y = pos.y + 6.0f;
                const bool plugin_close_hot = in_rect(plugin_close_x, plugin_close_y, 22.0f, 22.0f);
                const bool resize_hot = in_rect(pos.x + plugin_width - 20.0f,
                                                pos.y + plugin_height - 20.0f,
                                                20.0f, 20.0f);
                if (clicked && dragging.empty() && plugin_close_hot) {
                    plugin_open[status.manifest.id] = false;
                    state_dirty = true;
                } else if (clicked && dragging.empty() && resize_hot) {
                    start_resize(status.manifest.id, plugin_width, plugin_height);
                }
                if (clicked && dragging.empty() && !plugin_close_hot && !resize_hot &&
                    in_rect(pos.x, pos.y, plugin_width, 34.0f) &&
                    !bossrun_toggle_hot)
                    start_drag(status.manifest.id, pos.x, pos.y);
                if (click_down && cursor_valid && dragging == status.manifest.id) {
                    pos.x = (float)cursor.x - drag_dx;
                    pos.y = (float)cursor.y - drag_dy;
                    clamp_window(pos.x, pos.y, plugin_width, plugin_height);
                }
                if (click_down && cursor_valid &&
                    dragging == "%" + status.manifest.id) {
                    pos.w = std::max(min_width,
                        resize_origin_w + (float)cursor.x - drag_dx);
                    pos.h = std::max(min_height,
                        resize_origin_h + (float)cursor.y - drag_dy);
                    pos.w = std::min(pos.w, width - pos.x);
                    pos.h = std::min(pos.h, height - pos.y);
                }                const auto title_icon_it = g_module_icon_textures.find(status.manifest.id);
                const int title_icon = title_icon_it != g_module_icon_textures.end()
                    ? title_icon_it->second : -1;
                const auto frame = fmk::draw_window_frame(
                    {pos.x, pos.y, plugin_width, plugin_height,
                     status.manifest.name.c_str(), title_icon,
                     g_close_icon_texture, 34.0f,
                     {0.04f,0.07f,0.09f,0.94f}, {0.10f,0.18f,0.23f,0.98f},
                     {0.30f,0.80f,0.70f,1.0f}, true, g_resize_icon_texture},
                    {cursor_valid, (float)cursor.x, (float)cursor.y, clicked});                if (clicked && plugin_close_hot) {
                    ++window_index;
                    continue;
                }

                auto commands = g_plugins->draw_commands(status.manifest.id);
                // Diagnostic guard removed.
                const auto canvas = g_plugins->canvas_size(status.manifest.id);
                const bool has_canvas = canvas.first > 0.0f && canvas.second > 0.0f;
                const float content_width = plugin_width;
                const float content_height = std::max(1.0f, plugin_height - 34.0f);
                const float raw_scale_x = has_canvas ? content_width / canvas.first : 1.0f;
                const float raw_scale_y = has_canvas ? content_height / canvas.second : 1.0f;
                
                // For map (has_canvas), scale uniformly to fill the window (max scale)
                const float scale_min = has_canvas ? std::max(raw_scale_x, raw_scale_y) : 1.0f;
                const float scale_x = scale_min;
                const float scale_y = scale_min;
                const float command_origin_x = pos.x +
                    (has_canvas ? (content_width - canvas.first * scale_x) * 0.5f : 0.0f);
                const float command_origin_y = pos.y + 34.0f +
                    (has_canvas ? (content_height - canvas.second * scale_y) * 0.5f : 0.0f);
                fmk::draw_set_clip(pos.x, pos.y + 34.0f,
                                   content_width, content_height);
                for (const auto& command : commands) {
                    const fmk::Color color{command.r, command.g, command.b, command.a};
                    if (command.type == fmk::DrawCommandType::Circle) {
                        fmk::draw_circle(command_origin_x + command.x1 * scale_x,
                                         command_origin_y + command.y1 * scale_y,
                                         command.radius * scale_min,
                                         command.thickness * scale_min,
                                         color, command.filled);
                    } else if (command.type == fmk::DrawCommandType::Line) {
                        fmk::draw_line(command_origin_x + command.x1 * scale_x,
                                       command_origin_y + command.y1 * scale_y,
                                       command_origin_x + command.x2 * scale_x,
                                       command_origin_y + command.y2 * scale_y,
                                       command.thickness * scale_min, color);
                    } else if (command.type == fmk::DrawCommandType::Rect) {
                        if (command.filled)
                            fmk::draw_rect(command_origin_x + command.x1 * scale_x,
                                           command_origin_y + command.y1 * scale_y,
                                           command.x2 * scale_x, command.y2 * scale_y,
                                           color);
                        else
                            fmk::draw_rect_outline(command_origin_x + command.x1 * scale_x,
                                           command_origin_y + command.y1 * scale_y,
                                           command.x2 * scale_x,
                                           command.y2 * scale_y,
                                           command.thickness * scale_min, color);                    } else if (command.type == fmk::DrawCommandType::Image) {
                        // Diagnostic guard removed.
                        const auto module_assets = g_module_asset_textures.find(status.manifest.id);
                        if (module_assets != g_module_asset_textures.end()) {
                            const auto texture = module_assets->second.find(command.asset);
                            if (texture != module_assets->second.end())
                                fmk::draw_image(texture->second,
                                    command_origin_x + command.x1 * scale_x,
                                    command_origin_y + command.y1 * scale_y,
                                    command.x2 * scale_x, command.y2 * scale_y,
                                    command.u0, command.v0, command.u1, command.v1, color);
                        }
                    }
                }
                fmk::draw_reset_clip();

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
                } else if (map_window) {
                    std::string map_status = "En attente des donnees du monde...";
                    std::string map_zoom = "1.00";
                    for (const auto& line : rendered) {
                        if (line.rfind("MAP_STATUS|", 0) == 0) {
                            std::string payload = line.substr(11);
                            size_t pipe = payload.find('|');
                            if (pipe != std::string::npos) {
                                map_status = payload.substr(0, pipe);
                                map_zoom = payload.substr(pipe + 1);
                            } else {
                                map_status = payload;
                            }
                        }
                    }
                    
                    const float map_button_w = 38.0f;
                    const float map_button_h = 38.0f;
                    const float map_button_y = pos.y + plugin_height - map_button_h - 10.0f;
                    const float map_gap = 10.0f;
                    const float map_buttons_x = pos.x + 10.0f;
                    const bool zoom_out_hot = in_rect(map_buttons_x, map_button_y, map_button_w, map_button_h);
                    const bool zoom_in_hot = in_rect(map_buttons_x + map_button_w + map_gap,
                                                     map_button_y, map_button_w, map_button_h);
                    const bool chest_hot = in_rect(map_buttons_x + 2 * (map_button_w + map_gap),
                                                   map_button_y, map_button_w, map_button_h);
                    const bool orb_hot = in_rect(map_buttons_x + 3 * (map_button_w + map_gap),
                                                 map_button_y, map_button_w, map_button_h);

                    auto map_asset_texture = [&](const char* name) {
                        const auto module_assets = g_module_asset_textures.find(status.manifest.id);
                        if (module_assets == g_module_asset_textures.end()) return -1;
                        const auto it = module_assets->second.find(name);
                        return it == module_assets->second.end() ? -1 : it->second;
                    };
                    const int zoom_out_icon = map_asset_texture("zoom-out.png");
                    const int zoom_in_icon = map_asset_texture("zoom-in.png");
                    const int chest_icon = map_asset_texture("chests.png");
                    const int orb_icon = map_asset_texture("orbs.png");
                    auto map_button = [&](float x, float w, const char* label, int texture, bool hot) {
                        if (texture >= 0)
                            fmk::draw_image(texture, x + 2.0f, map_button_y + 2.0f,
                                            w - 4.0f, map_button_h - 4.0f, 0, 0, 1, 1,
                                            hot ? fmk::Color{1.0f,1.0f,0.78f,1.0f}
                                                : fmk::Color{1,1,1,1});
                        else
                            button(x, map_button_y, w, label, hot,
                                   {0.15f,0.30f,0.46f,1.0f});
                    };
                    map_button(map_buttons_x, map_button_w, "-", zoom_out_icon, zoom_out_hot);
                    map_button(map_buttons_x + map_button_w + map_gap, map_button_w,
                               "+", zoom_in_icon, zoom_in_hot);
                    map_button(map_buttons_x + 2 * (map_button_w + map_gap), map_button_w,
                               "Coffres", chest_icon, chest_hot);
                    map_button(map_buttons_x + 3 * (map_button_w + map_gap), map_button_w,
                               "Orbes", orb_icon, orb_hot);
                    if (clicked && dragging.empty()) {
                        if (zoom_out_hot) g_plugins->dispatch_event("map_zoom_out");
                        else if (zoom_in_hot) g_plugins->dispatch_event("map_zoom_in");
                        else if (chest_hot) g_plugins->dispatch_event("map_chests");
                        else if (orb_hot) g_plugins->dispatch_event("map_orbs");
                    }
                    
                    // Draw zoom indicator (top right)
                    std::string zoom_str = "x" + map_zoom;
                    const float zoom_w = fmk::measure_text(16.0f, zoom_str.c_str());
                    const float zoom_x = pos.x + plugin_width - zoom_w - 20.0f;
                    const float zoom_y = pos.y + 44.0f; // Just below title bar
                    fmk::draw_rect(zoom_x - 6.0f, zoom_y - 2.0f, zoom_w + 12.0f, 24.0f, {0.0f,0.0f,0.0f,0.8f});
                    fmk::draw_text(zoom_x, zoom_y + 2.0f, 16.0f, {1.0f,1.0f,1.0f,1.0f}, zoom_str.c_str());

                    // The module image is clipped to content, so redraw the
                    // frame edge and resize grip after it to keep the chrome
                    // visually above the map.
                    fmk::draw_rect_outline(pos.x, pos.y, plugin_width, plugin_height,
                                           1.5f, {0.30f,0.80f,0.70f,1.0f});
                    fmk::draw_rect(pos.x, pos.y, plugin_width, 34.0f,
                                   {0.06f,0.12f,0.16f,0.98f});
                    if (title_icon >= 0)
                        fmk::draw_image(title_icon, pos.x + 6.0f, pos.y + 4.0f,
                                        26.0f, 26.0f, 0, 0, 1, 1,
                                        {1,1,1,1});
                    
                    std::string full_title = status.manifest.name;
                    if (!map_status.empty()) {
                        full_title += " - " + map_status;
                    }
                    fmk::draw_text(pos.x + 40.0f, pos.y + 7.0f, 20.0f,
                                   {0.95f,0.97f,1.0f,1.0f},
                                   full_title.c_str());
                    if (g_close_icon_texture >= 0)
                        fmk::draw_image(g_close_icon_texture, plugin_close_x,
                                        plugin_close_y, 22.0f, 22.0f,
                                        0, 0, 1, 1, {1,1,1,1});
                    if (g_resize_icon_texture >= 0)
                        fmk::draw_image(g_resize_icon_texture,
                                        pos.x + plugin_width - 18.0f,
                                        pos.y + plugin_height - 18.0f,
                                        16.0f, 16.0f, 0, 0, 1, 1,
                                        {1,1,1,1});
                } else if (report_window) {
                    std::string report_status = "En attente d'un personnage...";
                    for (const auto& line : rendered) {
                        if (line.rfind("REPORT_STATUS|", 0) == 0)
                            report_status = line.substr(14);
                    }
                    fmk::draw_text(pos.x + 14.0f, pos.y + 44.0f, 16.0f,
                                   {0.68f,0.80f,0.74f,1.0f}, report_status.c_str());
                    const float save_x = pos.x + 14.0f;
                    const float save_y = pos.y + 76.0f;
                    const float save_w = 200.0f;
                    const bool save_hot = in_rect(save_x, save_y, save_w, 30.0f);
                    button(save_x, save_y, save_w, "Sauvegarder maintenant", save_hot,
                           {0.16f,0.42f,0.30f,1.0f});
                    const float link_x = save_x + save_w + 12.0f;
                    const float link_w = plugin_width - (link_x - pos.x) - 14.0f;
                    const bool link_hot = in_rect(link_x, save_y, link_w, 30.0f);
                    button(link_x, save_y, link_w, "Ouvrir le rapport", link_hot,
                           {0.15f,0.30f,0.46f,1.0f});
                    if (clicked && dragging.empty() && save_hot)
                        g_plugins->memory().request_report_export();
                    if (clicked && dragging.empty() && link_hot)
                        fmk::report_open();

                    const unsigned long saved_tick = fmk::report_last_saved_tick();
                    std::string saved = "Derniere sauvegarde : aucune cette session";
                    if (saved_tick) {
                        const unsigned long seconds = (GetTickCount() - saved_tick) / 1000;
                        saved = seconds < 2 ? "Derniere sauvegarde : a l'instant"
                            : "Derniere sauvegarde : il y a " + std::to_string(seconds) + " s";
                    }
                    fmk::draw_text(pos.x + 14.0f, pos.y + 118.0f, 14.0f,
                                   {0.55f,0.65f,0.70f,1.0f}, saved.c_str());                } else if (console_window) {
                    const std::size_t visible_lines = (std::size_t)std::max(
                        1.0f, std::floor((plugin_height - 73.0f) / 21.0f));
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
                    const float scroll_w = fmk::measure_text(13.0f, scroll_info.c_str());
                    const float scroll_x = plugin_close_x - 10.0f - scroll_w;
                    fmk::draw_text(scroll_x, pos.y + 10, 13.0f,
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
            // Independent companion window for the saved BossRun history.
            bool bossrun_enabled = false;
            for (const auto& status : statuses) {
                if (status.manifest.id == "patobeur.bossrun") {
                    bossrun_enabled = status.enabled;
                    break;
                }
            }
            if (bossrun_enabled && world_available && bossrun_history_open) {
                if (bossrun_history_window.w <= 0.0f) bossrun_history_window.w = 400.0f;
                if (bossrun_history_window.h <= 0.0f) bossrun_history_window.h = 110.0f;
                const float history_w = bossrun_history_window.w;
                const float history_h = bossrun_history_window.h;
                const auto main_it = plugin_windows.find("patobeur.bossrun");
                if (!bossrun_history_moved && main_it != plugin_windows.end()) {
                    bossrun_history_window.x = main_it->second.x + 412.0f;
                    bossrun_history_window.y = main_it->second.y;
                    clamp_window(bossrun_history_window.x, bossrun_history_window.y,
                                 history_w, history_h);
                }
                bossrun_history_initialized = true;
                const float hx = bossrun_history_window.x;
                const float hy = bossrun_history_window.y;
                const float close_x = hx + history_w - 30.0f;
                const float close_y = hy + 6.0f;
                const bool close_hot = in_rect(close_x, close_y, 22.0f, 22.0f);
                const bool history_resize_hot = in_rect(hx + history_w - 20.0f,
                                                        hy + history_h - 20.0f,
                                                        20.0f, 20.0f);
                if (clicked && dragging.empty() && close_hot) {
                    bossrun_history_open = false;
                    state_dirty = true;
                } else if (clicked && dragging.empty() && history_resize_hot) {
                    start_resize("bossrun-history", history_w, history_h);
                } else if (clicked && dragging.empty() &&
                           in_rect(hx, hy, history_w, 34.0f)) {
                    start_drag("#bossrun-history", hx, hy);
                }
                if (click_down && cursor_valid && dragging == "%bossrun-history") {
                    bossrun_history_window.w = std::max(280.0f,
                        resize_origin_w + (float)cursor.x - drag_dx);
                    bossrun_history_window.h = std::max(80.0f,
                        resize_origin_h + (float)cursor.y - drag_dy);
                    bossrun_history_window.w = std::min(bossrun_history_window.w,
                                                        width - bossrun_history_window.x);
                    bossrun_history_window.h = std::min(bossrun_history_window.h,
                                                        height - bossrun_history_window.y);
                }
                if (click_down && cursor_valid && dragging == "#bossrun-history") {
                    const float nx = (float)cursor.x - drag_dx;
                    const float ny = (float)cursor.y - drag_dy;
                    if (std::abs(nx - bossrun_history_window.x) > 2.0f ||
                        std::abs(ny - bossrun_history_window.y) > 2.0f)
                        bossrun_history_moved = true;
                    bossrun_history_window.x = nx;
                    bossrun_history_window.y = ny;
                    clamp_window(bossrun_history_window.x, bossrun_history_window.y,
                                 history_w, history_h);
                }
                const auto history_icon_it =
                    g_module_icon_textures.find("patobeur.bossrun");
                const int history_icon = history_icon_it != g_module_icon_textures.end()
                    ? history_icon_it->second : -1;
                fmk::draw_window_frame(
                    {bossrun_history_window.x, bossrun_history_window.y,
                     history_w, history_h, "Derniers temps", history_icon,
                     g_close_icon_texture, 34.0f,
                     {0.04f,0.07f,0.09f,0.94f}, {0.10f,0.18f,0.23f,0.98f},
                     {0.30f,0.80f,0.70f,1.0f}, true, g_resize_icon_texture},
                    {cursor_valid, (float)cursor.x, (float)cursor.y, clicked});            }

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