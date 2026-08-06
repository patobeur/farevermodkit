#include "paths.h"
#include "dashboard.h"
#include "navigator.h"
#include "routes.h"
#include "players.h"
#include <filesystem>
#include <cstring>

namespace fmk {
const std::wstring& game_dir() {
    static const std::wstring value = (std::filesystem::current_path().wstring() + L"\\");
    return value;
}
const std::wstring& data_dir() {
    static const std::wstring value = (std::filesystem::current_path() / L"farevermodkit" / L"assets" / L"atlas").wstring() + L"\\";
    return value;
}
std::wstring character_data_dir(const std::string&, const std::string&, const std::string&) { return data_dir(); }
bool dashboard_take_save_request() { return false; }
void dashboard_observe(const Collection&, const Inventories&, const std::vector<JobState>&, const RuneState&, const CompletionState&, const std::vector<WeaponMastery>&) {}
bool dashboard_has_changes() { return false; }
void dashboard_save(const Collection&, const Inventories&, const std::vector<JobState>&, const RuneState&, const CompletionState&, const std::vector<WeaponMastery>&) {}
void dashboard_mark_saved() {}
void routes_init() {}
void routes_tick() {}
void routes_draw(const InputState&,bool,float,float,float,float) {}
int routes_count() { return 0; }
void players_init() {}
void players_poll() {}
void players_draw(const InputState&,bool,float,float,float,float) {}
int players_count() { return 0; }
}