import os

def fix_mod_folder_path(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    old = 'void report_open_mod_folder() { if(!g_module_dir.empty()) ShellExecuteW(nullptr,L"explore",g_module_dir.c_str(),nullptr,nullptr,SW_SHOWNORMAL); }'
    new = 'void report_open_mod_folder() { ShellExecuteW(nullptr,L"explore",data_dir().c_str(),nullptr,nullptr,SW_SHOWNORMAL); }'

    if old in text:
        text = text.replace(old, new)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)
        print("Updated mod folder path")
    else:
        print("Target not found")

fix_mod_folder_path(r'd:\farever-mods\farevermodkit\native\report.cpp')
