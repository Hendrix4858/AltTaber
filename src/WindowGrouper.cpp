#include "WindowGrouper.h"
#include "ActivationHistory.h"
#include "utils/Util.h"
#include "utils/PwaDetector.h"
#include "core/ConfigManager.h"
#include <algorithm>

namespace WindowGrouper {

    QList<WindowGroup> groupWindows(const QList<WindowDescriptor>& windows,
                                    const ActivationHistory* history,
                                    HWND selfHwnd) {
        QList<WindowGroup> result;
        QMap<QString, int> indexMap;

        bool pwaEnabled = cfg().getPwaEnabled();
        bool separateGroups = pwaEnabled && cfg().getPwaMode() == PwaMode::SeparateGroups;

        for (const auto& desc : windows) {
            if (desc.hwnd == selfHwnd) continue;
            if (desc.processPath.isEmpty()) continue;

            QString groupKey = desc.processPath;
            if (separateGroups && desc.windowKind == WindowKind::Pwa)
                groupKey = desc.appUserModelId;

            if (auto it = indexMap.find(groupKey); it != indexMap.end()) {
                result[it.value()].addWindow(
                    {desc.title, desc.className, desc.appUserModelId, desc.hwnd, desc.windowKind});
            } else {
                WindowGroup group;
                group.exePath = desc.processPath;
                if (desc.windowKind == WindowKind::Pwa) {
                    group.icon = PwaDetector::getPwaIcon(desc.hwnd, desc.appUserModelId, desc.processPath);
                } else {
                    group.icon = Util::getCachedIcon(desc.processPath, desc.hwnd);
                }
                group.addWindow(
                    {desc.title, desc.className, desc.appUserModelId, desc.hwnd, desc.windowKind});
                indexMap[groupKey] = result.size();
                result.append(group);
            }
        }

        if (history) {
            std::sort(result.begin(), result.end(),
                      [history](const WindowGroup& a, const WindowGroup& b) {
                qint64 latestA = 0, latestB = 0;
                for (const auto& win : a.windows)
                    latestA = qMax(latestA, history->lastActivationTime(win.hwnd));
                for (const auto& win : b.windows)
                    latestB = qMax(latestB, history->lastActivationTime(win.hwnd));
                return latestA > latestB;
            });
        }

        return result;
    }

} // namespace WindowGrouper
