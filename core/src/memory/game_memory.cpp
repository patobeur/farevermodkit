#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

#include "game_memory.h"
#include "hl_runtime.h"
#include "memory_log.h"
#include "offsets.gen.h"

namespace fmk {

namespace {
std::filesystem::path boss_tracking_path() {
    wchar_t local[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    const std::filesystem::path root = n && n < MAX_PATH
        ? std::filesystem::path(local) / L"farevermodkit"
        : std::filesystem::current_path() / L"farevermodkit-user-data";
    return root / L"settings" / L"bossrun-tracked.txt";
}
}

GameMemory::GameMemory() {
    load_boss_tracking_settings();
}

void GameMemory::load_boss_tracking_settings() {
    std::ifstream input(boss_tracking_path(), std::ios::binary);
    std::string kind;
    while (std::getline(input, kind)) {
        if (!kind.empty() && kind.back() == '\r') kind.pop_back();
        if (kind.empty() || kind.size() > 160) continue;
        if (std::all_of(kind.begin(), kind.end(), [](unsigned char c) {
                return c >= 0x20 && c != 0x7f;
            })) enabled_extra_kinds_.insert(kind);
    }
    memory_log("bossrun: restored %zu tracked ordinary monster kind(s)",
               enabled_extra_kinds_.size());
}

void GameMemory::save_boss_tracking_settings_locked() const {
    const auto path = boss_tracking_path();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        memory_log("bossrun: cannot create centralized settings directory");
        return;
    }
    std::vector<std::string> kinds(enabled_extra_kinds_.begin(),
                                   enabled_extra_kinds_.end());
    std::sort(kinds.begin(), kinds.end());
    const auto temporary = path.wstring() + L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return;
        for (const auto& kind : kinds) output << kind << '\n';
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        memory_log("bossrun: failed to save tracked ordinary monsters");
    }
}

bool GameMemory::configure_build_hash(const std::string& observed_hash) {
    std::lock_guard lock(mutex_);
    configured_build_hash_ = observed_hash;
    const std::string expected = FMK_BUILD_SHA256;
    build_validated_ = observed_hash.size() == expected.size() &&
        std::equal(observed_hash.begin(), observed_hash.end(), expected.begin(),
                   [](char a, char b) {
                       return std::tolower(static_cast<unsigned char>(a)) ==
                              std::tolower(static_cast<unsigned char>(b));
                   });
    status_.build_validated = build_validated_;
    memory_log("memory: build hash %s", build_validated_ ? "validated" : "rejected");
    return build_validated_;
}

const char* GameMemory::expected_build_hash() {
    return FMK_BUILD_SHA256;
}

bool GameMemory::probe(bool allow_scan) {
    std::lock_guard lock(mutex_);
    if (!build_validated_) {
        memory_log("memory: probe refused; build hash is not validated");
        return false;
    }
    mem_flush_cache();
    status_.probe_count++;
    status_.app_found = reader_locate_app(allow_scan);
    status_.hero_found = status_.app_found && reader_locate_hero(false);
    status_.loading = status_.app_found ? reader_is_loading() : true;
    status_.in_world = status_.hero_found && !status_.loading;
    memory_log("memory: probe app=%d hero=%d loading=%d world=%d scan=%d",
               status_.app_found ? 1 : 0, status_.hero_found ? 1 : 0,
               status_.loading ? 1 : 0, status_.in_world ? 1 : 0,
               allow_scan ? 1 : 0);
    return status_.app_found || status_.hero_found;
}

bool GameMemory::refresh() {
    std::lock_guard lock(mutex_);
    if (!build_validated_) return false;
    const uint64_t now = GetTickCount64();
    if (last_refresh_ms_ != 0 && now - last_refresh_ms_ < 500) {
        return status_.in_world;
    }
    last_refresh_ms_ = now;
    const bool previous_world = status_.in_world;
    mem_flush_cache();
    status_.app_found = reader_locate_app(false);
    status_.hero_found = status_.app_found && reader_locate_hero(false);
    status_.loading = status_.app_found ? reader_is_loading() : true;
    status_.in_world = status_.app_found && status_.hero_found && !status_.loading;
    if (previous_world != status_.in_world) {
        memory_log("memory: world state changed app=%d hero=%d loading=%d world=%d",
                   status_.app_found ? 1 : 0, status_.hero_found ? 1 : 0,
                   status_.loading ? 1 : 0, status_.in_world ? 1 : 0);
    }
    return status_.in_world;
}

void GameMemory::reset() {
    std::lock_guard lock(mutex_);
    reader_reset();
    status_ = {};
    configured_build_hash_.clear();
    build_validated_ = false;
    status_.loading = true;
}

MemoryStatus GameMemory::status() const {
    std::lock_guard lock(mutex_);
    return status_;
}

bool GameMemory::available() const {
    std::lock_guard lock(mutex_);
    return status_.in_world;
}

#define FMK_MEMORY_READ(method, reader, type) \
bool GameMemory::method(type* out) { \
    if (!out) return false; \
    std::lock_guard lock(mutex_); \
    if (!status_.in_world) return false; \
    mem_flush_cache(); \
    return reader(out); \
}

FMK_MEMORY_READ(read_collection, reader_read_collection, Collection)
FMK_MEMORY_READ(read_inventories, reader_read_inventories, Inventories)
FMK_MEMORY_READ(read_runes, reader_read_runes, RuneState)
FMK_MEMORY_READ(read_completion, reader_read_completion, CompletionState)
FMK_MEMORY_READ(read_loot, reader_read_loot_state, LootState)
FMK_MEMORY_READ(read_map, reader_read_map_state, MapState)
FMK_MEMORY_READ(read_chatbox, reader_read_chatbox, ChatBoxState)
FMK_MEMORY_READ(read_roster, reader_read_roster, RosterState)
FMK_MEMORY_READ(read_unit_state, reader_read_unit_state, UnitState)

bool GameMemory::read_boss_state(BossState* out) {
    if (!out) return false;
    std::lock_guard lock(mutex_);
    if (!status_.in_world) return false;
    mem_flush_cache();
    if (!reader_read_boss_state(out)) return false;
    out->tracked = out->is_boss || (!out->kind.empty() && enabled_extra_kinds_.find(out->kind) != enabled_extra_kinds_.end());
    return true;
}

bool GameMemory::boss_tracking_enabled(const std::string& kind) const {
    std::lock_guard lock(mutex_);
    return !kind.empty() && enabled_extra_kinds_.find(kind) != enabled_extra_kinds_.end();
}

void GameMemory::set_boss_tracking_enabled(const std::string& kind, bool enabled) {
    if (kind.empty()) return;
    std::lock_guard lock(mutex_);
    if (enabled) enabled_extra_kinds_.insert(kind);
    else enabled_extra_kinds_.erase(kind);
    save_boss_tracking_settings_locked();
    memory_log("bossrun: ordinary monster %s tracking=%d", kind.c_str(),
               enabled ? 1 : 0);
}
#undef FMK_MEMORY_READ

bool GameMemory::read_jobs(std::vector<JobState>* out) {
    if (!out) return false;
    std::lock_guard lock(mutex_);
    if (!status_.in_world) return false;
    mem_flush_cache();
    return reader_read_jobs(out);
}

bool GameMemory::read_unit_progress(std::vector<UnitProgress>* out) {
    if (!out) return false;
    std::lock_guard lock(mutex_);
    if (!status_.in_world) return false;
    mem_flush_cache();
    return reader_read_unit_progress(out);
}

bool GameMemory::read_weapon_mastery(std::vector<WeaponMastery>* out) {
    if (!out) return false;
    std::lock_guard lock(mutex_);
    if (!status_.in_world) return false;
    mem_flush_cache();
    return reader_read_weapon_mastery(out);
}

bool GameMemory::read_chat(int32_t from, int32_t max,
                           std::vector<ChatMessage>* out, int32_t* total) {
    if (!out || !total) return false;
    std::lock_guard lock(mutex_);
    if (!status_.in_world) return false;
    mem_flush_cache();
    return reader_read_chat(from, max, out, total);
}

bool GameMemory::read_hero_pose(double* x, double* y, double* z, double* rot_z) {
    if (!x || !y || !z || !rot_z) return false;
    std::lock_guard lock(mutex_);
    if (!status_.in_world) return false;
    mem_flush_cache();
    return reader_read_hero_pose(x, y, z, rot_z);
}

bool GameMemory::read_nearby_entities(double radius, std::vector<NearbyEntity>* out) {
    if (!out || radius <= 0 || radius > 2000) return false;
    std::lock_guard lock(mutex_);
    if (!status_.in_world) return false;
    mem_flush_cache();
    return reader_read_nearby_entities(radius, out);
}

bool GameMemory::read_world_name(std::string* out) {
    if (!out) return false;
    std::lock_guard lock(mutex_);
    if (!status_.in_world) return false;
    mem_flush_cache();
    return reader_read_world_name(out);
}

void GameMemory::update_map_snapshot(const MapSnapshot& snapshot) {
    std::lock_guard lock(mutex_);
    map_snapshot_ = snapshot;
}

MapSnapshot GameMemory::map_snapshot() const {
    std::lock_guard lock(mutex_);
    return map_snapshot_;
}

bool GameMemory::read_camera(double* px, double* py, double* pz,
                             double* tx, double* ty, double* tz) {
    if (!px || !py || !pz || !tx || !ty || !tz) return false;
    std::lock_guard lock(mutex_);
    if (!status_.in_world) return false;
    mem_flush_cache();
    return reader_read_camera(px, py, pz, tx, ty, tz);
}

} // namespace fmk

void fmk::GameMemory::request_report_export() {
    report_export_requested_.store(true, std::memory_order_release);
}

bool fmk::GameMemory::take_report_export_request() {
    return report_export_requested_.exchange(false, std::memory_order_acq_rel);
}
