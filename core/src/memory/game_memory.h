#pragma once

#include "hl_reader.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>

namespace fmk {

struct MemoryStatus {
    bool app_found = false;
    bool hero_found = false;
    bool loading = true;
    bool in_world = false;
    uint64_t probe_count = 0;
    bool build_validated = false;
};

// The single read-only gateway used by FareverModKit. It owns the lifecycle
// around the legacy reader globals and makes a read cycle explicit: flush the
// VirtualQuery cache, probe the live GameApp/Hero, then ask for a snapshot.
// No method writes to the game process and no probe is started implicitly by
// plugin discovery. The native host decides when the startup delay has made
// a scan safe.
class GameMemory {
public:
    GameMemory();
    GameMemory(const GameMemory&) = delete;
    GameMemory& operator=(const GameMemory&) = delete;

    // Validate the hlboot.dat hash used by the generated offsets. No probe
    // is allowed until this succeeds.
    bool configure_build_hash(const std::string& observed_hash);
    static const char* expected_build_hash();

    // Probe the game object graph. `allow_scan` enables the expensive fallback
    // instance scan; callers should keep it false during game startup.
    bool probe(bool allow_scan = false);
    // Refreshes only the cached App/Hero roots; it never starts a heap scan.
    bool refresh();
    void reset();

    MemoryStatus status() const;
    bool available() const;

    bool read_collection(Collection* out);
    bool read_inventories(Inventories* out);
    bool read_jobs(std::vector<JobState>* out);
    bool read_runes(RuneState* out);
    bool read_completion(CompletionState* out);
    bool read_unit_progress(std::vector<UnitProgress>* out);
    bool read_weapon_mastery(std::vector<WeaponMastery>* out);
    bool read_loot(LootState* out);
    bool read_map(MapState* out);
    bool read_chat(int32_t from, int32_t max, std::vector<ChatMessage>* out,
                   int32_t* total);
    bool read_chatbox(ChatBoxState* out);
    bool read_roster(RosterState* out);
    bool read_unit_state(UnitState* out);
    bool read_boss_state(BossState* out);
    bool boss_tracking_enabled(const std::string& kind) const;
    void set_boss_tracking_enabled(const std::string& kind, bool enabled);
    bool read_hero_pose(double* x, double* y, double* z, double* rot_z);
    // Report generation is requested by Lua and consumed by the existing
    // atlas worker, so no disk export or broad snapshot runs on Present.
    void request_report_export();
    bool take_report_export_request();

    bool read_camera(double* px, double* py, double* pz,
                     double* tx, double* ty, double* tz);

private:
    void load_boss_tracking_settings();
    void save_boss_tracking_settings_locked() const;

    mutable std::mutex mutex_;
    std::string configured_build_hash_;
    bool build_validated_ = false;
    uint64_t last_refresh_ms_ = 0;
    MemoryStatus status_{};
    std::unordered_set<std::string> enabled_extra_kinds_;
    std::atomic<bool> report_export_requested_{false};
};

} // namespace fmk
