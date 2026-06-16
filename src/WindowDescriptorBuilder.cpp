#include "WindowDescriptorBuilder.h"
#include "utils/Util.h"
#include "utils/PwaDetector.h"
#include <QFileInfo>

WindowDescriptor WindowDescriptorBuilder::fromHwnd(HWND hwnd) {
    WindowDescriptor desc;
    desc.hwnd = hwnd;
    desc.title = Util::getWindowTitle(hwnd);
    desc.className = Util::getClassName(hwnd);
    desc.processPath = Util::getWindowProcessPath(hwnd);
    desc.processName = QFileInfo(desc.processPath).fileName();
    desc.appUserModelId = PwaDetector::getAppUserModelId(hwnd);
    if (PwaDetector::isPwaWindow(desc.processPath, desc.appUserModelId))
        desc.windowKind = WindowKind::Pwa;
    return desc;
}
