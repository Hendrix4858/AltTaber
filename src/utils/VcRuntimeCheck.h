#ifndef WIN_SWITCHER_VCRUNTIMECHECK_H
#define WIN_SWITCHER_VCRUNTIMECHECK_H

namespace VcRuntimeCheck {

// Download URL of the VC++ Redistributable matching the running architecture.
const wchar_t* vcRedistDownloadUrl();

// Returns true when the MSVC runtime DLLs can be loaded.
bool isRuntimeAvailable();

} // namespace VcRuntimeCheck

#endif // WIN_SWITCHER_VCRUNTIMECHECK_H
