#define WIN32_LEAN_AND_MEAN
#include "../src/plugin_host.h"

#include <iostream>
#include <string>

int wmain(int argc, wchar_t** argv) {
    if (argc != 4 && argc != 5) {
        std::wcerr << L"usage: plugin_host_smoke.exe <modules> <lua.dll> <sha256> [locale]\n";
        return 2;
    }
    std::string hash;
    for (const wchar_t character : std::wstring(argv[3])) {
        if (character > 0x7f) return 2;
        hash.push_back(static_cast<char>(character));
    }
    std::string locale = "en-US";
    if (argc == 5) {
        locale.clear();
        for (const wchar_t character : std::wstring(argv[4])) {
            if (character > 0x7f) return 2;
            locale.push_back(static_cast<char>(character));
        }
    }
    fmk::PluginHost host(argv[1]);
    if (!host.load_all(argv[2], hash, locale)) {
        std::cerr << "no plugin loaded\n";
    }
    for (const auto& status : host.statuses()) {
        std::cout << status.manifest.id << " loaded=" << status.loaded
                  << " enabled=" << status.enabled;
        if (!status.error.empty()) std::cout << " error=" << status.error;
        std::cout << "\n";
    }
    host.dispatch_event("smoke");
    host.render();
    const auto rendered = host.rendered_text("patobeur.bravo");
    for (const auto& text : rendered) std::cout << "rendered: " << text << "\n";
    if (!host.set_enabled("patobeur.bravo", false)) {
        std::cerr << "disable failed\n";
        return 1;
    }
    if (!host.set_enabled("patobeur.bravo", true)) {
        std::cerr << "enable failed\n";
        return 1;
    }
    if (!host.set_enabled("blaakan.inventory", true)) {
        std::cerr << "inventory enable failed\n";
        return 1;
    }
    host.render();
    const auto inventory_text = host.rendered_text("blaakan.inventory");
    if (!inventory_text.empty()) {
        std::cerr << "world-only inventory rendered outside the game\n";
        return 1;
    }
    std::cout << "inventory correctly deferred until a character enters the world\n";
    host.set_enabled("blaakan.inventory", false);
    host.shutdown();
    std::cout << "Plugin host smoke test passed\n";
    return 0;
}