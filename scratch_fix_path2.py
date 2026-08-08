import os

def fix_path(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    old = 'void report_open_mod_folder() { ShellExecuteW(nullptr,L"explore",data_dir().c_str(),nullptr,nullptr,SW_SHOWNORMAL); }'
    new = 'void report_open_mod_folder() { auto p=std::filesystem::path(game_dir())/L"farevermodkit"; ShellExecuteW(nullptr,L"explore",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL); }'

    if old in text:
        text = text.replace(old, new)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)
        print("Fixed report_open_mod_folder in report.cpp")
    else:
        print("Target not found in report.cpp")

fix_path(r'd:\farever-mods\farevermodkit\native\report.cpp')
