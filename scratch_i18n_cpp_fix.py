import os, re

with open(r'd:\farever-mods\farevermodkit\native\modules\report_ui.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

new_logic = '''
std::string detect_game_locale_ui() {
    HANDLE h = CreateFileW(L"options.ini", GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return "en-US";
    char buf[65536] = {0};
    DWORD n = 0;
    bool ok = ReadFile(h, buf, sizeof(buf) - 1, &n, nullptr);
    CloseHandle(h);
    if (!ok) return "en-US";
    std::string txt(buf, n);
    for (char& c : txt) c = (char)tolower((unsigned char)c);
    if (txt.find("\\"language\\" : \\"fr\\"") != std::string::npos || txt.find("\\"language\\":\\"fr\\"") != std::string::npos || txt.find("\\"language\\" :\\"fr\\"") != std::string::npos || txt.find("\\"language\\": \\"fr\\"") != std::string::npos) return "fr-FR";
    if (txt.find("\\"language\\" : \\"es\\"") != std::string::npos || txt.find("\\"language\\":\\"es\\"") != std::string::npos || txt.find("\\"language\\" :\\"es\\"") != std::string::npos || txt.find("\\"language\\": \\"es\\"") != std::string::npos) return "es-ES";
    return "en-US";
}

static const char* t(const char* fr, const char* en, const char* es) {
    static std::string loc = detect_game_locale_ui();
    if (loc == "fr-FR") return fr;
    if (loc == "es-ES") return es;
    return en;
}
'''

text = re.sub(r'std::string detect_game_locale\(\);[\s\S]*?return en;\n\}', new_logic, text)

with open(r'd:\farever-mods\farevermodkit\native\modules\report_ui.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
