#include "paths.h"
#include "dashboard.h"
#include "navigator.h"
#include "routes.h"
#include "players.h"
#include <filesystem>
#include <cstring>
#include <windows.h>

namespace fmk {
const std::wstring& game_dir() {
    static const std::wstring value = (std::filesystem::current_path().wstring() + L"\\");
    return value;
}
const std::wstring& data_dir() {
    static const std::wstring value = (std::filesystem::current_path() / L"farevermodkit" / L"assets" / L"atlas").wstring() + L"\\";
    return value;
}
const std::wstring& user_data_dir() {
    static const std::wstring value = [] {
        wchar_t local[MAX_PATH] = {};
        const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
        std::filesystem::path root = n && n < MAX_PATH
            ? std::filesystem::path(local) / L"farevermodkit"
            : std::filesystem::current_path() / L"farevermodkit-user-data";
        std::error_code ec;
        std::filesystem::create_directories(root / L"settings", ec);
        std::filesystem::create_directories(root / L"data", ec);
        std::filesystem::create_directories(root / L"logs", ec);
        return root.wstring() + L"\\";
    }();
    return value;
}

std::wstring safe_component(const std::string& input) {
    std::wstring out;
    for (unsigned char c : input) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') out.push_back((wchar_t)c);
        else out.push_back(L'_');
    }
    if (out.empty() || out == L"." || out == L"..") out = L"unknown";
    return out;
}

std::wstring account_data_dir(const std::string& account_uuid) {
    const auto path = std::filesystem::path(user_data_dir()) / L"data" / L"accounts" /
                      safe_component(account_uuid);
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return path.wstring() + L"\\";
}
std::wstring character_data_dir(const std::string& account_uuid,
                                const std::string& character_id,
                                const std::string&) {
    const auto path = std::filesystem::path(user_data_dir()) / L"data" / L"accounts" /
                      safe_component(account_uuid) / L"characters" /
                      safe_component(character_id);
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return path.wstring() + L"\\";
}
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