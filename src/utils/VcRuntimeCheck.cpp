#include "utils/VcRuntimeCheck.h"
#include "utils/RuntimeArch.h"
#include <windows.h>

namespace VcRuntimeCheck {

namespace {
constexpr wchar_t kVcRedistX64[] = L"https://aka.ms/vs/17/release/vc_redist.x64.exe";
constexpr wchar_t kVcRedistArm64[] = L"https://aka.ms/vs/17/release/vc_redist.arm64.exe";
}

const wchar_t* vcRedistDownloadUrl() {
    switch (detectRuntimeArch()) {
    case RuntimeArchitecture::Arm64: return kVcRedistArm64;
    case RuntimeArchitecture::X64:
    default:                         return kVcRedistX64;
    }
}

namespace {
bool dllPresent(const wchar_t* name) {
    HMODULE handle = LoadLibraryW(name);
    if (handle) {
        FreeLibrary(handle);
        return true;
    }
    return false;
}
}

bool isRuntimeAvailable() {
    return dllPresent(L"vcruntime140.dll")
        && dllPresent(L"vcruntime140_1.dll")
        && dllPresent(L"msvcp140.dll");
}

} // namespace VcRuntimeCheck
