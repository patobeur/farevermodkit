import os

def update_report_cpp(path):
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()

    target = 'void report_open(){ auto p=std::filesystem::path(user_data_dir())/L"html"/L"farever-report.html"; ShellExecuteW(nullptr,L"open",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL); }'

    new_code = target + '''
void report_open_data_folder() { auto p=std::filesystem::path(user_data_dir())/L"html"; ShellExecuteW(nullptr,L"explore",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL); }
void report_open_mod_folder() { if(!g_module_dir.empty()) ShellExecuteW(nullptr,L"explore",g_module_dir.c_str(),nullptr,nullptr,SW_SHOWNORMAL); }
'''

    if target in text:
        text = text.replace(target, new_code)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)
        print("Updated report.cpp")
    else:
        print("Target not found in report.cpp")

update_report_cpp(r'd:\farever-mods\farevermodkit\native\report.cpp')
