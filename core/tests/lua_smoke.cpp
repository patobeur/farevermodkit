#define WIN32_LEAN_AND_MEAN
#include "../src/lua_runtime.h"

#include <iostream>
#include <string>

int wmain(int argc, wchar_t** argv) {
    if (argc != 4 && argc != 5) {
        std::wcerr << L"usage: lua_smoke.exe <lua.dll> <sha256> <script>\n";
        return 2;
    }
    std::string expected_hash;
    for (const wchar_t character : std::wstring(argv[2])) {
        if (character > 0x7f) {
            std::wcerr << L"hash must be ASCII hex\n";
            return 2;
        }
        expected_hash.push_back(static_cast<char>(character));
    }

    fmk::LuaRuntime runtime;
    if (!runtime.load_engine(argv[1], expected_hash)) {
        std::cerr << "load failed: " << runtime.last_error() << "\n";
        return 1;
    }
    const std::filesystem::path script = argv[3];
    const auto language = script.parent_path() / "languages" / "en-US.json";
    if (std::filesystem::is_regular_file(language) && !runtime.load_language_file(language)) {
        std::cerr << "language failed: " << runtime.last_error() << "\n";
        return 1;
    }
    if (!runtime.execute_file(script)) {
        std::cerr << "script failed: " << runtime.last_error() << "\n";
        return 1;
    }
    if (argc == 5) {
        runtime.clear_rendered_text();
        if (!runtime.call_callback("on_render")) {
            std::cerr << "callback failed: " << runtime.last_error() << "\n";
            return 1;
        }
        for (const auto& text : runtime.rendered_text()) {
            std::cout << "rendered: " << text << "\n";
        }
    }
    std::cout << "Lua smoke test passed\n";
    return 0;
}