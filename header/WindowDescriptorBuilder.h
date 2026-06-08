#ifndef WIN_SWITCHER_WINDOWDESCRIPTORBUILDER_H
#define WIN_SWITCHER_WINDOWDESCRIPTORBUILDER_H

#include <Windows.h>
#include "WindowTypes.h"

class WindowDescriptorBuilder {
public:
    static WindowDescriptor fromHwnd(HWND hwnd);
};

#endif //WIN_SWITCHER_WINDOWDESCRIPTORBUILDER_H
