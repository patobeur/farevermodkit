#define WIN32_LEAN_AND_MEAN
#include "lua_runtime.h"

#include <windows.h>
#include <bcrypt.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace fmk {
namespace {
struct lua_State;
struct lua_Debug;
using NewState = lua_State* (*)();
using OpenLibs = void (*)(lua_State*);
using LoadFile = int (*)(lua_State*, const char*, const char*);
using KFunction = int (*)(lua_State*, int, std::intptr_t);
using PCall = int (*)(lua_State*, int, int, int, std::intptr_t, KFunction);
using Close = void (*)(lua_State*);
using CFunction = int (*)(lua_State*);
using PushCClosure = void (*)(lua_State*, CFunction, int);
using PushNil = void (*)(lua_State*);
using PushBoolean = void (*)(lua_State*, int);
using PushInteger = void (*)(lua_State*, long long);
using PushString = const char* (*)(lua_State*, const char*);
using ToString = const char* (*)(lua_State*, int, std::size_t*);
using CreateTable = void (*)(lua_State*, int, int);
using SetField = void (*)(lua_State*, int, const char*);
using SetGlobal = void (*)(lua_State*, const char*);
using SetTop = void (*)(lua_State*, int);
using GetTop = int (*)(lua_State*);
using GetGlobal = int (*)(lua_State*, const char*);

using Hook = void (*)(lua_State*, lua_Debug*);
using SetHook = int (*)(lua_State*, Hook, int, int);
using RaiseError = int (*)(lua_State*);

template <typename T>
T lua_proc(HMODULE library, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(library, name));
}

std::string sha256_file(const std::filesystem::path& path) {
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
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_length, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }
    std::vector<char> buffer(64 * 1024);
    while (input.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || input.gcount() > 0) {
        const auto bytes = static_cast<ULONG>(input.gcount());
        if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), bytes, 0) != 0) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return {};
        }
    }
    const NTSTATUS finish = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (finish != 0) return {};

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto byte : digest) output << std::setw(2) << static_cast<unsigned int>(byte);
    return output.str();
}

bool valid_sha256(const std::string& value) {
    if (value.size() != 64) return false;
    for (const auto character : value) {
        const bool digit = character >= '0' && character <= '9';
        const bool lower = character >= 'a' && character <= 'f';
        const bool upper = character >= 'A' && character <= 'F';
        if (!digit && !lower && !upper) return false;
    }
    return true;
}

// The callbacks only use API functions resolved from the same Lua DLL. They
// never receive a native pointer or a reference to the game's memory.
PushString g_push_string = nullptr;
PushNil g_push_nil = nullptr;
PushBoolean g_push_boolean = nullptr;
PushInteger g_push_integer = nullptr;
CreateTable g_create_table = nullptr;
SetField g_set_field = nullptr;
ToString g_to_string = nullptr;
LuaRuntime* g_runtime = nullptr;
thread_local RaiseError g_raise_error = nullptr;
thread_local int g_instruction_ticks = 0;

int imgui_text(lua_State* state) {
    const char* text = g_to_string ? g_to_string(state, 1, nullptr) : nullptr;
    if (g_runtime && text) g_runtime->capture_text(text);
    return 0;
}

void set_bool_field(lua_State* state, const char* name, bool value) {
    if (!g_push_boolean || !g_set_field) return;
    g_push_boolean(state, value ? 1 : 0);
    g_set_field(state, -2, name);
}

void set_integer_field(lua_State* state, const char* name, long long value) {
    if (!g_push_integer || !g_set_field) return;
    g_push_integer(state, value);
    g_set_field(state, -2, name);
}

void set_string_field(lua_State* state, const char* name, const std::string& value) {
    if (!g_push_string || !g_set_field) return;
    g_push_string(state, value.c_str());
    g_set_field(state, -2, name);
}

int farever_memory_status(lua_State* state) {
    if (!g_create_table || !g_runtime) return 0;
    g_create_table(state, 0, 7);
    auto* memory = g_runtime->memory_context();
    if (memory) memory->refresh();
    const MemoryStatus status = memory ? memory->status() : MemoryStatus{};
    set_bool_field(state, "appFound", status.app_found);
    set_bool_field(state, "heroFound", status.hero_found);
    set_bool_field(state, "loading", status.loading);
    set_bool_field(state, "inWorld", status.in_world);
    set_bool_field(state, "buildValidated", status.build_validated);
    set_integer_field(state, "probeCount", static_cast<long long>(status.probe_count));
    set_bool_field(state, "available", memory && memory->available());
    return 1;
}

int farever_player(lua_State* state) {
    if (!g_create_table || !g_runtime) return 0;
    auto* memory = g_runtime->memory_context();
    if (!memory) {
        if (g_push_nil) g_push_nil(state);
        return 1;
    }
    Inventories inventories;
    if (!memory->read_inventories(&inventories)) {
        if (g_push_nil) g_push_nil(state);
        return 1;
    }
    g_create_table(state, 0, 8);
    set_string_field(state, "name", inventories.character);
    set_string_field(state, "characterUuid", inventories.character_uuid);
    set_string_field(state, "accountId", inventories.steam_account_id);
    set_string_field(state, "class", inventories.hero_class);
    set_string_field(state, "activeWeapon", inventories.active_weapon);
    set_integer_field(state, "level", inventories.character_level);
    set_integer_field(state, "experience", inventories.experience);
    set_integer_field(state, "bankSlots", inventories.bank_slots);
    return 1;
}

int farever_boss(lua_State* state) {
    if (!g_create_table || !g_runtime) return 0;
    auto* memory = g_runtime->memory_context();
    static BossState cached;
    static ULONGLONG last_read_ms = 0;
    const ULONGLONG now_ms = GetTickCount64();
    if (!memory || (now_ms - last_read_ms >= 100 && !memory->read_boss_state(&cached))) {
        if (g_push_nil) g_push_nil(state);
        return 1;
    }
    if (now_ms - last_read_ms >= 100) last_read_ms = now_ms;
    const BossState& boss = cached;
    g_create_table(state, 0, 8);
    set_bool_field(state, "valid", boss.valid);
    set_bool_field(state, "present", boss.present);
    set_bool_field(state, "inCombat", boss.in_combat);
    set_bool_field(state, "defeated", boss.defeated);
    set_bool_field(state, "tracked", boss.tracked);
    set_bool_field(state, "isBoss", boss.is_boss);
    set_string_field(state, "kind", boss.kind);
    set_string_field(state, "runtimeClass", boss.runtime_class);
    set_integer_field(state, "health", static_cast<long long>(boss.health));
    set_integer_field(state, "nowMs", static_cast<long long>(now_ms));
    return 1;
}
std::string safe_storage_component(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') out.push_back((char)c);
        else out.push_back('_');
    }
    if (out.empty() || out == "." || out == "..") out = "unknown";
    if (out.size() > 96) out.resize(96);
    return out;
}

std::string json_escape(const std::string& value) {
    std::string out;
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') { out.push_back('\\'); out.push_back((char)c); }
        else if (c >= 0x20) out.push_back((char)c);
    }
    return out;
}

bool bossrun_profile(GameMemory* memory, const std::string& kind,
                     Inventories* identity, std::filesystem::path* path) {
    if (!memory || kind.empty() || !identity || !path ||
        !memory->read_inventories(identity)) return false;
    wchar_t local[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    if (!n || n >= MAX_PATH) return false;
    *path = std::filesystem::path(local) / L"farevermodkit" / L"data" / L"accounts" /
            safe_storage_component(identity->steam_account_id) / L"characters" /
            safe_storage_component(identity->character_uuid) / L"bossrun" /
            (safe_storage_component(kind) + ".json");
    return true;
}

long long lua_integer_arg(lua_State* state, int index) {
    const char* value = g_to_string ? g_to_string(state, index, nullptr) : nullptr;
    return value ? _strtoi64(value, nullptr, 10) : 0;
}

long long json_integer(const std::string& text, const char* key) {
    const std::regex pattern(std::string("\\\"") + key + "\\\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    return std::regex_search(text, match, pattern) ? _strtoi64(match[1].str().c_str(), nullptr, 10) : 0;
}

int farever_bossrun_load(lua_State* state) {
    const char* raw_kind = g_to_string ? g_to_string(state, 1, nullptr) : nullptr;
    if (!raw_kind || !*raw_kind || !g_runtime || !g_create_table) {
        if (g_push_nil) g_push_nil(state); return 1;
    }
    Inventories identity;
    std::filesystem::path path;
    if (!bossrun_profile(g_runtime->memory_context(), raw_kind, &identity, &path)) {
        if (g_push_nil) g_push_nil(state); return 1;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) { if (g_push_nil) g_push_nil(state); return 1; }
    std::ostringstream buffer; buffer << input.rdbuf();
    const std::string json = buffer.str();
    g_create_table(state, 0, 6);
    set_integer_field(state, "lastMs", json_integer(json, "lastMs"));
    set_integer_field(state, "bestMs", json_integer(json, "bestMs"));
    set_integer_field(state, "totalMs", json_integer(json, "totalMs"));
    set_integer_field(state, "kills", json_integer(json, "kills"));
    set_integer_field(state, "wipes", json_integer(json, "wipes"));
    return 1;
}

int farever_bossrun_save(lua_State* state) {
    const char* raw_kind = g_to_string ? g_to_string(state, 1, nullptr) : nullptr;
    const char* raw_class = g_to_string ? g_to_string(state, 2, nullptr) : nullptr;
    if (!raw_kind || !*raw_kind || !g_runtime) return 0;
    const long long last_ms = lua_integer_arg(state, 3);
    const long long best_ms = lua_integer_arg(state, 4);
    const long long total_ms = lua_integer_arg(state, 5);
    const long long kills = lua_integer_arg(state, 6);
    const long long wipes = lua_integer_arg(state, 7);
    Inventories identity;
    std::filesystem::path path;
    if (!bossrun_profile(g_runtime->memory_context(), raw_kind, &identity, &path)) return 0;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return 0;
    const auto updated = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream json;
    json << "{\n"
         << "  \"accountId\": \"" << json_escape(identity.steam_account_id) << "\",\n"
         << "  \"characterUuid\": \"" << json_escape(identity.character_uuid) << "\",\n"
         << "  \"characterName\": \"" << json_escape(identity.character) << "\",\n"
         << "  \"bossKind\": \"" << json_escape(raw_kind) << "\",\n"
         << "  \"runtimeClass\": \"" << json_escape(raw_class ? raw_class : "") << "\",\n"
         << "  \"lastMs\": " << last_ms << ",\n"
         << "  \"bestMs\": " << best_ms << ",\n"
         << "  \"totalMs\": " << total_ms << ",\n"
         << "  \"kills\": " << kills << ",\n"
         << "  \"wipes\": " << wipes << ",\n"
         << "  \"updatedAtUnixMs\": " << updated << "\n}\n";
    const auto temporary = path.wstring() + L".tmp";
    { std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      if (!output) return 0; output << json.str(); }
    MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    return 0;
}
int farever_inventory_summary(lua_State* state) {
    if (!g_create_table || !g_runtime) return 0;
    auto* memory = g_runtime->memory_context();
    Inventories inventories;
    if (!memory || !memory->read_inventories(&inventories)) {
        if (g_push_nil) g_push_nil(state);
        return 1;
    }
    g_create_table(state, 0, 5);
    set_integer_field(state, "bank", static_cast<long long>(inventories.bank.size()));
    set_integer_field(state, "bankEquipment", static_cast<long long>(inventories.bank_equipment.size()));
    set_integer_field(state, "equipped", static_cast<long long>(inventories.equipped.size()));
    set_integer_field(state, "bags", static_cast<long long>(inventories.bags.size()));
    set_integer_field(state, "bankSlots", inventories.bank_slots);
    return 1;
}
int i18n(lua_State* state) {
    // Until the language catalog is wired, return the key itself. This keeps
    // plugins deterministic and gives the UI a useful fallback string.
    const char* key = g_to_string ? g_to_string(state, 1, nullptr) : nullptr;
    if (!g_push_string) return 0;
    std::string translated = g_runtime && key ? g_runtime->translate(key) : std::string();
    if (translated.empty() && key) translated = key;
    g_push_string(state, translated.c_str());
    return 1;
}

void instruction_hook(lua_State* state, lua_Debug*) {
    if (g_instruction_ticks <= 0 || !g_push_string || !g_raise_error) return;
    if (--g_instruction_ticks != 0) return;
    g_push_string(state, "Lua plugin instruction budget exceeded");
    g_raise_error(state);
}

void remove_global(HMODULE library, lua_State* state, const char* name) {
    const auto push_nil = lua_proc<PushNil>(library, "lua_pushnil");
    const auto set_global = lua_proc<SetGlobal>(library, "lua_setglobal");
    if (push_nil && set_global) {
        push_nil(state);
        set_global(state, name);
    }
}

} // namespace

LuaRuntime::~LuaRuntime() {
    if (!library_) return;
    const auto close = lua_proc<Close>((HMODULE)library_, "lua_close");
    if (close && state_) close((lua_State*)state_);
    FreeLibrary((HMODULE)library_);
    if (g_runtime == this) g_runtime = nullptr;
    g_push_string = nullptr;
    g_push_nil = nullptr;
    g_push_boolean = nullptr;
    g_push_integer = nullptr;
    g_create_table = nullptr;
    g_set_field = nullptr;
    g_to_string = nullptr;
    g_raise_error = nullptr;
    g_instruction_ticks = 0;
}

bool LuaRuntime::install_sandbox() {
    const HMODULE library = (HMODULE)library_;
    const auto push_cclosure = lua_proc<PushCClosure>(library, "lua_pushcclosure");
    const auto create_table = lua_proc<CreateTable>(library, "lua_createtable");
    const auto set_field = lua_proc<SetField>(library, "lua_setfield");
    const auto set_global = lua_proc<SetGlobal>(library, "lua_setglobal");
    g_to_string = lua_proc<ToString>(library, "lua_tolstring");
    g_push_string = lua_proc<PushString>(library, "lua_pushstring");
    g_push_nil = lua_proc<PushNil>(library, "lua_pushnil");
    g_push_boolean = lua_proc<PushBoolean>(library, "lua_pushboolean");
    g_push_integer = lua_proc<PushInteger>(library, "lua_pushinteger");
    g_create_table = create_table;
    g_set_field = set_field;
    if (!push_cclosure || !create_table || !set_field || !set_global ||
        !g_to_string || !g_push_string || !g_push_nil ||
        !g_push_boolean || !g_push_integer) {
        error_ = "Lua is missing sandbox API exports";
        return false;
    }

    // luaL_openlibs is used only during initialization. These globals are
    // removed before any third-party file is compiled or executed.
    constexpr const char* forbidden[] = {
        "os", "io", "debug", "package", "require", "dofile", "loadfile",
        "load", "collectgarbage"
    };
    for (const auto* name : forbidden) remove_global(library, (lua_State*)state_, name);

    // Minimal, pointer-free host API. The native overlay will replace the
    // imgui callback body once the DXGI core is connected.
    create_table((lua_State*)state_, 0, 1);
    push_cclosure((lua_State*)state_, &imgui_text, 0);
    set_field((lua_State*)state_, -2, "text");
    set_global((lua_State*)state_, "imgui");

    push_cclosure((lua_State*)state_, &i18n, 0);
    set_global((lua_State*)state_, "i18n");

    create_table((lua_State*)state_, 0, 6);
    push_cclosure((lua_State*)state_, &farever_memory_status, 0);
    set_field((lua_State*)state_, -2, "memory_status");
    push_cclosure((lua_State*)state_, &farever_player, 0);
    set_field((lua_State*)state_, -2, "player");
    push_cclosure((lua_State*)state_, &farever_inventory_summary, 0);
    set_field((lua_State*)state_, -2, "inventory_summary");
    push_cclosure((lua_State*)state_, &farever_boss, 0);
    set_field((lua_State*)state_, -2, "boss");    push_cclosure((lua_State*)state_, &farever_bossrun_load, 0);
    set_field((lua_State*)state_, -2, "bossrun_load");
    push_cclosure((lua_State*)state_, &farever_bossrun_save, 0);
    set_field((lua_State*)state_, -2, "bossrun_save");
    set_global((lua_State*)state_, "farever");
    sandbox_ready_ = true;
    return true;
}

bool LuaRuntime::load_engine(const std::filesystem::path& dll_path, const std::string& expected_sha256) {
    error_.clear();
    if (library_) { error_ = "Lua engine already loaded"; return false; }
    if (!std::filesystem::is_regular_file(dll_path)) {
        error_ = "configured Lua engine DLL does not exist";
        return false;
    }
    if (!valid_sha256(expected_sha256)) {
        error_ = "Lua engine SHA-256 is missing or malformed";
        return false;
    }
    const std::string actual_sha256 = sha256_file(dll_path);
    if (actual_sha256.empty()) {
        error_ = "could not calculate Lua engine SHA-256";
        return false;
    }
    for (std::size_t i = 0; i < actual_sha256.size(); ++i) {
        char expected = expected_sha256[i];
        if (expected >= 'A' && expected <= 'F') expected = static_cast<char>(expected - 'A' + 'a');
        if (expected != actual_sha256[i]) {
            error_ = "Lua engine SHA-256 does not match the configured value";
            return false;
        }
    }

    HMODULE lib = LoadLibraryW(dll_path.c_str());
    if (!lib) { error_ = "could not load Lua engine DLL"; return false; }
    const auto new_state = lua_proc<NewState>(lib, "luaL_newstate");
    const auto open_libs = lua_proc<OpenLibs>(lib, "luaL_openlibs");
    const auto close = lua_proc<Close>(lib, "lua_close");
    if (!new_state || !open_libs || !close) {
        FreeLibrary(lib);
        error_ = "Lua engine is missing required exports";
        return false;
    }
    lua_State* state = new_state();
    if (!state) {
        FreeLibrary(lib);
        error_ = "Lua could not create a state";
        return false;
    }

    library_ = lib;
    state_ = state;
    g_runtime = this;
    // Open the standard library in a private state, then immediately remove
    // every capability that can access the OS, filesystem, process, or module
    // loader. No plugin code runs before install_sandbox() succeeds.
    open_libs(state);
    if (!install_sandbox()) {
        close(state);
        FreeLibrary(lib);
        library_ = nullptr;
        state_ = nullptr;
        sandbox_ready_ = false;
        if (g_runtime == this) g_runtime = nullptr;
        g_push_string = nullptr;
    g_push_nil = nullptr;
    g_push_boolean = nullptr;
    g_push_integer = nullptr;
    g_create_table = nullptr;
    g_set_field = nullptr;
    g_to_string = nullptr;
    g_raise_error = nullptr;
        return false;
    }
    return true;
}

bool LuaRuntime::execute_file(const std::filesystem::path& script) {
    error_.clear();
    g_runtime = this;
    if (!library_ || !state_ || !sandbox_ready_) {
        error_ = "Lua sandbox is not ready";
        return false;
    }
    if (!std::filesystem::is_regular_file(script)) {
        error_ = "Lua plugin entry file does not exist";
        return false;
    }
    const HMODULE library = (HMODULE)library_;
    const auto load_file = lua_proc<LoadFile>(library, "luaL_loadfilex");
    const auto pcall = lua_proc<PCall>(library, "lua_pcallk");
    const auto to_string = lua_proc<ToString>(library, "lua_tolstring");
    const auto set_top = lua_proc<SetTop>(library, "lua_settop");
    const auto get_top = lua_proc<GetTop>(library, "lua_gettop");
    const auto set_hook = lua_proc<SetHook>(library, "lua_sethook");
    g_raise_error = lua_proc<RaiseError>(library, "lua_error");
    if (!load_file || !pcall || !to_string || !set_top || !get_top ||
        !set_hook || !g_raise_error) {
        error_ = "Lua engine is missing execution exports";
        return false;
    }

    const auto utf8 = script.u8string();
    const std::string path(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    if (load_file((lua_State*)state_, path.c_str(), nullptr) != 0) {
        const char* message = get_top((lua_State*)state_) > 0
            ? to_string((lua_State*)state_, -1, nullptr) : nullptr;
        error_ = message ? message : "Lua failed to compile plugin script";
        set_top((lua_State*)state_, 0);
        return false;
    }

    // Protected call: an error in one plugin cannot unwind into the overlay.
    // LUA_MASKCOUNT is 8 in Lua 5.4. The hook runs every 1,000 VM instructions
    // and aborts after 10,000 ticks (about 10 million instructions).
    constexpr int lua_mask_count = 8;
    g_instruction_ticks = 10000;
    set_hook((lua_State*)state_, &instruction_hook, lua_mask_count, 1000);
    const int result = pcall((lua_State*)state_, 0, 0, 0, 0, nullptr);
    set_hook((lua_State*)state_, nullptr, 0, 0);
    g_instruction_ticks = 0;
    if (result != 0) {
        const char* message = get_top((lua_State*)state_) > 0
            ? to_string((lua_State*)state_, -1, nullptr) : nullptr;
        error_ = message ? message : "Lua plugin execution failed";
        set_top((lua_State*)state_, 0);
        return false;
    }
    set_top((lua_State*)state_, 0);
    return true;
}

bool LuaRuntime::load_language_file(const std::filesystem::path& language_file) {
    error_.clear();
    g_runtime = this;
    if (!std::filesystem::is_regular_file(language_file)) {
        error_ = "language catalog does not exist";
        return false;
    }
    std::ifstream input(language_file, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();
    const std::regex pair_pattern("\\\"([^\\\"]+)\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::unordered_map<std::string, std::string> parsed;
    for (std::sregex_iterator it(text.begin(), text.end(), pair_pattern), end; it != end; ++it) {
        parsed[(*it)[1].str()] = (*it)[2].str();
    }
    if (parsed.empty()) {
        error_ = "language catalog has no string entries";
        return false;
    }
    translations_ = std::move(parsed);
    return true;
}
bool LuaRuntime::call_event(const char* name) {
    error_.clear();
    g_runtime = this;
    if (!name || !*name) {
        error_ = "Lua event name is empty";
        return false;
    }
    if (!library_ || !state_ || !sandbox_ready_) {
        error_ = "Lua sandbox is not ready";
        return false;
    }
    const HMODULE library = (HMODULE)library_;
    const auto get_global = lua_proc<GetGlobal>(library, "lua_getglobal");
    const auto pcall = lua_proc<PCall>(library, "lua_pcallk");
    const auto push_string = lua_proc<PushString>(library, "lua_pushstring");
    const auto push_nil = lua_proc<PushNil>(library, "lua_pushnil");
    const auto to_string = lua_proc<ToString>(library, "lua_tolstring");
    const auto set_top = lua_proc<SetTop>(library, "lua_settop");
    const auto set_hook = lua_proc<SetHook>(library, "lua_sethook");
    g_raise_error = lua_proc<RaiseError>(library, "lua_error");
    if (!get_global || !pcall || !push_string || !push_nil || !to_string ||
        !set_top || !set_hook || !g_raise_error) {
        error_ = "Lua engine is missing event exports";
        return false;
    }

    // LUA_TFUNCTION is 6 in Lua 5.4. Missing event handlers are optional.
    const int callback_type = get_global((lua_State*)state_, "on_event");
    if (callback_type != 6) {
        set_top((lua_State*)state_, 0);
        return true;
    }
    push_string((lua_State*)state_, name);
    push_nil((lua_State*)state_);
    constexpr int lua_mask_count = 8;
    g_instruction_ticks = 10000;
    set_hook((lua_State*)state_, &instruction_hook, lua_mask_count, 1000);
    const int result = pcall((lua_State*)state_, 2, 0, 0, 0, nullptr);
    set_hook((lua_State*)state_, nullptr, 0, 0);
    g_instruction_ticks = 0;
    if (result != 0) {
        const char* message = to_string((lua_State*)state_, -1, nullptr);
        error_ = message ? message : "Lua event callback failed";
        set_top((lua_State*)state_, 0);
        return false;
    }
    set_top((lua_State*)state_, 0);
    return true;
}
std::string LuaRuntime::translate(const std::string& key) const {
    const auto it = translations_.find(key);
    return it == translations_.end() ? std::string() : it->second;
}

void LuaRuntime::capture_text(const char* text) {
    if (text) rendered_text_.emplace_back(text);
}

bool LuaRuntime::call_callback(const char* name) {
    error_.clear();
    g_runtime = this;
    if (!name || !*name) {
        error_ = "Lua callback name is empty";
        return false;
    }
    if (!library_ || !state_ || !sandbox_ready_) {
        error_ = "Lua sandbox is not ready";
        return false;
    }
    const HMODULE library = (HMODULE)library_;
    const auto get_global = lua_proc<GetGlobal>(library, "lua_getglobal");

    const auto pcall = lua_proc<PCall>(library, "lua_pcallk");
    const auto to_string = lua_proc<ToString>(library, "lua_tolstring");
    const auto set_top = lua_proc<SetTop>(library, "lua_settop");
    const auto set_hook = lua_proc<SetHook>(library, "lua_sethook");
    g_raise_error = lua_proc<RaiseError>(library, "lua_error");
    if (!get_global || !pcall || !to_string || !set_top ||
        !set_hook || !g_raise_error) {
        error_ = "Lua engine is missing callback exports";
        return false;
    }

    // LUA_TFUNCTION is 6 in Lua 5.4. Missing callbacks are optional.
    const int callback_type = get_global((lua_State*)state_, name);
    if (callback_type != 6) {
        set_top((lua_State*)state_, 0);
        return true;
    }

    constexpr int lua_mask_count = 8;
    g_instruction_ticks = 10000;
    set_hook((lua_State*)state_, &instruction_hook, lua_mask_count, 1000);
    const int result = pcall((lua_State*)state_, 0, 0, 0, 0, nullptr);
    set_hook((lua_State*)state_, nullptr, 0, 0);
    g_instruction_ticks = 0;
    if (result != 0) {
        const char* message = to_string((lua_State*)state_, -1, nullptr);
        error_ = message ? message : "Lua callback failed";
        set_top((lua_State*)state_, 0);
        return false;
    }
    set_top((lua_State*)state_, 0);
    return true;
}

} // namespace fmk