#pragma once
#include <windows.h>
#include <cstring>
#include <cwchar>
#include <string>

namespace kharvox {
inline std::wstring runtimeDirectory();
// Validated Beta renderer defaults; no deployment marker files required.
namespace rendererDefaults {
inline constexpr bool earlyXrRelease=true, descriptorReuse=true, freshShadows=true,
    gpuInputCopies=true, uncachedShadows=true, compiledImagePlans=true, snapshotBatch=true;
}
inline std::wstring logPath(const wchar_t* name) {
    std::wstring file(name);
    auto directory = runtimeDirectory() + L"\\logs\\";
    CreateDirectoryW((runtimeDirectory() + L"\\logs").c_str(), nullptr);
    const auto dot=file.find_last_of(L'.');
    if(dot!=std::wstring::npos&&file.substr(dot)==L".log") {
        for(size_t i=0;i<dot;++i) {
            if(file[i]==L'_')file[i]=L'-';
            else if(file[i]>=L'a'&&file[i]<=L'z')file[i]-=L'a'-L'A';
        }
        if(file.rfind(L"KHARVOX-",0)!=0)file=L"KHARVOX-"+file;
    } else {
        directory+=L"diagnostics\\";
        CreateDirectoryW(directory.c_str(),nullptr);
    }
    return directory+file;
}
inline std::string logPathA(const char* name) {
    std::wstring file;
    while(*name)file.push_back(static_cast<unsigned char>(*name++));
    const auto wide=logPath(file.c_str());
    const int size=WideCharToMultiByte(CP_UTF8,0,wide.c_str(),-1,nullptr,0,nullptr,nullptr);
    std::string result(size,'\0');
    if(size)WideCharToMultiByte(CP_UTF8,0,wide.c_str(),-1,result.data(),size,nullptr,nullptr);
    if(!result.empty())result.pop_back();
    return result;
}
inline std::wstring runtimeDirectory() {
    HMODULE module{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&runtimeDirectory), &module);
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(module, path, MAX_PATH);
    std::wstring result(path);
    const auto slash = result.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : result.substr(0, slash);
}
// Portable user-owned configuration lives BESIDE NativeBridge, never in it.
// Stock, immutable *_default.cfg and the bundled hand-model asset stay next
// to KharvoxLayer.dll. Runtime marker files and executable assets are
// unchanged. All custom *.cfg files (including weapon calibrations) are
// redirected to the one visible top-level UserData folder.
inline std::wstring userDataDirectory(){
    const auto runtime=runtimeDirectory();
    const auto slash=runtime.find_last_of(L"\\/");
    const std::wstring basename=slash==std::wstring::npos?runtime:runtime.substr(slash+1);
    const bool bridge=_wcsicmp(basename.c_str(),L"NativeBridge")==0;
    const auto root=bridge&&slash!=std::wstring::npos?runtime.substr(0,slash):runtime;
    const auto user=root+L"\\UserData";
    CreateDirectoryW(user.c_str(),nullptr);
    return user;
}
inline bool isUserConfigName(const wchar_t* name) noexcept {
    if(!name)return false;
    const std::wstring value(name);
    // No redirects for nested immutable assets or paths containing "..".
    if(value.find_first_of(L"\\/")!=std::wstring::npos
       ||value.find(L"..")!=std::wstring::npos
       ||value.size()<5)return false;
    const auto suffix=value.substr(value.size()-4);
    if(_wcsicmp(suffix.c_str(),L".cfg")!=0)return false;
    if(value.size()>=12
       &&_wcsicmp(value.c_str()+value.size()-12,L"_default.cfg")==0)
        return false;
    // This is a bundled source asset, not a per-user hand calibration.
    if(_wcsicmp(value.c_str(),L"hand_models.cfg")==0
       ||_wcsicmp(value.c_str(),L"two_hand_weapon_profiles_captured.cfg")==0
       ||_wcsicmp(value.c_str(),L"offhand_right_hand_positions_latest.cfg")==0)
        return false;
    return true;
}
inline std::wstring runtimePath(const wchar_t* name) {
    if(isUserConfigName(name))return userDataDirectory()+L"\\"+name;
    return runtimeDirectory()+L"\\"+name;
}
inline std::string runtimePathA(const char* name) {
    std::wstring file;
    while (*name) file.push_back(static_cast<unsigned char>(*name++));
    const auto wide = runtimePath(file.c_str());
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
    if (!result.empty()) result.pop_back();
    return result;
}
inline bool runtimeFileExists(const wchar_t* name) { return GetFileAttributesW(runtimePath(name).c_str()) != INVALID_FILE_ATTRIBUTES; }
}
