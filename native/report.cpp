#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <shellapi.h>
#include "paths.h"
#include "report.h"
namespace fmk {
void host_log(const char*, ...);
namespace {
struct File { std::wstring name; unsigned long long size, time; };
std::wstring dir(){ return user_data_dir()+L"data\\accounts\\"; }
std::filesystem::path g_module_dir;
volatile LONG g_last_saved_tick = 0;
bool info(const std::wstring&d,const std::wstring&n,File&f){WIN32_FILE_ATTRIBUTE_DATA a{};if(!GetFileAttributesExW((d+n).c_str(),GetFileExInfoStandard,&a))return false;f={n,((unsigned long long)a.nFileSizeHigh<<32)|a.nFileSizeLow,((unsigned long long)a.ftLastWriteTime.dwHighDateTime<<32)|a.ftLastWriteTime.dwLowDateTime};return true;}
std::vector<File> find(const std::wstring&d,const wchar_t*g){
 std::vector<File> v;
 std::function<void(const std::wstring&,const std::wstring&)> walk;
 walk=[&](const std::wstring&base,const std::wstring&rel){
  WIN32_FIND_DATAW x{}; HANDLE h=FindFirstFileW((base+g).c_str(),&x);
  if(h!=INVALID_HANDLE_VALUE){do{if(!(x.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){File f;if(info(base,x.cFileName,f)){f.name=rel+x.cFileName;v.push_back(f);}}}while(FindNextFileW(h,&x));FindClose(h);}
  h=FindFirstFileW((base+L"*").c_str(),&x);
  if(h!=INVALID_HANDLE_VALUE){do{if((x.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&x.cFileName[0]!=L'.')walk(base+std::wstring(x.cFileName)+L"\\",rel+std::wstring(x.cFileName)+L"\\");}while(FindNextFileW(h,&x));FindClose(h);}
 };
 walk(d,L"");
 std::sort(v.begin(),v.end(),[](const File&a,const File&b){return a.name<b.name;}); return v;
}
bool read(const std::wstring&p,std::string&s){HANDLE h=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return false;LARGE_INTEGER z{};bool ok=GetFileSizeEx(h,&z)&&z.QuadPart<=67108864;if(ok){s.resize((size_t)z.QuadPart);DWORD n=0;ok=s.empty()||(ReadFile(h,s.data(),(DWORD)s.size(),&n,nullptr)&&n==s.size());}CloseHandle(h);return ok;}
void quote(std::string&o,const std::string&s){o+='"';for(unsigned char c:s){if(c=='\\')o+="\\\\";else if(c=='"')o+="\\\"";else if(c=='\r')o+="\\r";else if(c=='\n')o+="\\n";else if(c=='\t')o+="\\t";else o+=(char)c;}o+='"';}
bool write(const std::wstring&p,const std::string&s){auto t=p+L".tmp";HANDLE h=CreateFileW(t.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);if(h==INVALID_HANDLE_VALUE)return false;DWORD n=0;bool ok=WriteFile(h,s.data(),(DWORD)s.size(),&n,nullptr)&&n==s.size()&&FlushFileBuffers(h);CloseHandle(h);if(ok)ok=MoveFileExW(t.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);if(!ok)DeleteFileW(t.c_str());return ok;}
}
void report_set_module_dir(const std::filesystem::path& path){ g_module_dir=path; }
bool report_install_assets(){
 if(g_module_dir.empty())return false; std::error_code ec;
 auto source=g_module_dir/L"html", target=std::filesystem::path(user_data_dir())/L"html";
 std::filesystem::create_directories(target,ec); ec.clear();
 std::filesystem::copy(source,target,std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing,ec);
 if(ec)return false;
 auto generated=std::filesystem::path(data_dir())/L"farever-atlas-icons.png";
 if(std::filesystem::is_regular_file(generated)){
  std::filesystem::create_directories(target/L"assets",ec); ec.clear();
  std::filesystem::copy_file(generated,target/L"assets"/L"farever-atlas-icons.png",
      std::filesystem::copy_options::overwrite_existing,ec);
 }
 return !ec;
}
void report_open(){ auto p=std::filesystem::path(user_data_dir())/L"html"/L"farever-report.html"; ShellExecuteW(nullptr,L"open",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL); }
void report_open_data_folder() { auto p=std::filesystem::path(user_data_dir())/L"html"; ShellExecuteW(nullptr,L"explore",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL); }
void report_open_mod_folder() { auto p=std::filesystem::path(game_dir())/L"farevermodkit"; ShellExecuteW(nullptr,L"explore",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL); }

unsigned long report_last_saved_tick() {
    return static_cast<unsigned long>(InterlockedCompareExchange(&g_last_saved_tick, 0, 0));
}
void report_refresh() {
    if (!report_install_assets()) {
        host_log("report: HTML assets unavailable");
        return;
    }
    const auto d = dir();
    const auto inventories = find(d, L"farever-inventory-*.json");
    const auto jobs = find(d, L"farever-jobs-*.json");
    const auto collections = find(d, L"farever-collection.json");
    File atlas{};
    const bool has_atlas = info(data_dir(), L"farever-atlas.tsv", atlas);
    unsigned long long signature = 1469598103934665603ull;
    auto mix = [&](const File& file) {
        signature ^= file.size; signature *= 1099511628211ull;
        signature ^= file.time; signature *= 1099511628211ull;
    };
    for (const auto& file : inventories) mix(file);
    for (const auto& file : jobs) mix(file);
    for (const auto& file : collections) mix(file);
    if (has_atlas) mix(atlas);
    static unsigned long long previous_signature = 0;

    SYSTEMTIME time{};
    GetLocalTime(&time);
    char date[32];
    sprintf_s(date, "%04u-%02u-%02u %02u:%02u:%02u", time.wYear,
              time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    std::string output = "// Generated by farever-modkit.\r\n"
                         "window.FAREVER_REPORT_DATA={generatedAt:\"" +
                         std::string(date) + "\"";
    std::string contents;
    auto append_array = [&](const char* key, const std::vector<File>& files) {
        output += "," + std::string(key) + ":[";
        bool first = true;
        for (const auto& file : files) {
            contents.clear();
            if (!read(d + file.name, contents)) continue;
            if (!first) output += ',';
            output += contents;
            first = false;
        }
        output += ']';
    };
    append_array("collections", collections);
    append_array("inventories", inventories);
    append_array("jobs", jobs);
    output += ",atlasTsv:";
    contents.clear();
    if (has_atlas && read(data_dir() + atlas.name, contents)) quote(output, contents);
    else output += "\"\"";
    output += "};\r\n";

    if (write(user_data_dir() + L"farever-report-data.js", output)) {
        previous_signature = signature;
        InterlockedExchange(&g_last_saved_tick, static_cast<LONG>(GetTickCount()));
        host_log("report: dashboard data updated");
    } else {
        host_log("report: dashboard write failed (%lu)", GetLastError());
    }
}

} // namespace fmk
