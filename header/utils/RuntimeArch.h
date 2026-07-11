#ifndef WIN_SWITCHER_RUNTIMEARCH_H
#define WIN_SWITCHER_RUNTIMEARCH_H

#include <cstdint>

enum class RuntimeArchitecture : uint8_t {
    X64,
    Arm64,
    X86,
    Unknown
};

RuntimeArchitecture detectRuntimeArch();
const char* runtimeArchToString(RuntimeArchitecture arch);

#endif // WIN_SWITCHER_RUNTIMEARCH_H
