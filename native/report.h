#pragma once
#include <filesystem>
namespace fmk {
void report_set_module_dir(const std::filesystem::path& path);
bool report_install_assets();
void report_refresh();
void report_open();
void report_open_data_folder();
void report_open_mod_folder();
unsigned long report_last_saved_tick();
}
