#include "utils/RuntimeArch.h"
#include <string_view>
#include <windows.h>

RuntimeArchitecture detectRuntimeArch()
{
    // Primary: read PE header of the current executable
    WCHAR path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
        HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            IMAGE_DOS_HEADER dos;
            DWORD read;
            if (ReadFile(hFile, &dos, sizeof(dos), &read, nullptr) && read == sizeof(dos)) {
                SetFilePointer(hFile, dos.e_lfanew, nullptr, FILE_BEGIN);
                DWORD sig;
                IMAGE_FILE_HEADER fh;
                if (ReadFile(hFile, &sig, sizeof(sig), &read, nullptr) && read == sizeof(sig)
                    && sig == IMAGE_NT_SIGNATURE
                    && ReadFile(hFile, &fh, sizeof(fh), &read, nullptr) && read == sizeof(fh)) {
                    CloseHandle(hFile);
                    switch (fh.Machine) {
                    case IMAGE_FILE_MACHINE_AMD64:  return RuntimeArchitecture::X64;
                    case IMAGE_FILE_MACHINE_ARM64:  return RuntimeArchitecture::Arm64;
                    case IMAGE_FILE_MACHINE_I386:   return RuntimeArchitecture::X86;
                    default:                        return RuntimeArchitecture::Unknown;
                    }
                }
            }
            CloseHandle(hFile);
        }
    }

    // Fallback: compile-time APP_ARCH
#ifdef APP_ARCH
    if (std::string_view(APP_ARCH) == "arm64") return RuntimeArchitecture::Arm64;
    if (std::string_view(APP_ARCH) == "win64") return RuntimeArchitecture::X64;
#endif
    return RuntimeArchitecture::Unknown;
}

const char* runtimeArchToString(RuntimeArchitecture arch)
{
    switch (arch) {
    case RuntimeArchitecture::X64:     return "x64";
    case RuntimeArchitecture::Arm64:   return "arm64";
    case RuntimeArchitecture::X86:     return "x86";
    case RuntimeArchitecture::Unknown: return "unknown";
    }
    return "unknown";
}
