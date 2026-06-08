#include "WindowGrouper.h"
#include "ActivationHistory.h"
#include "utils/Util.h"
#include <algorithm>

namespace WindowGrouper {

    QList<WindowGroup> groupWindows(const QList<WindowDescriptor>& windows,
                                    const ActivationHistory* history,
                                    HWND selfHwnd) {
        QList<WindowGroup> result;
        QMap<QString, int> pathIndex;

        for (const auto& desc : windows) {
            if (desc.hwnd == selfHwnd) continue;
            if (desc.processPath.isEmpty()) continue;

            if (auto it = pathIndex.find(desc.processPath); it != pathIndex.end()) {
                result[it.value()].addWindow(
                    {desc.title, desc.className, desc.hwnd});
            } else {
                WindowGroup group;
                group.exePath = desc.processPath;
                group.icon = Util::getCachedIcon(desc.processPath, desc.hwnd);
                group.addWindow(
                    {desc.title, desc.className, desc.hwnd});
                pathIndex[desc.processPath] = result.size();
                result.append(group);
            }
        }

        if (history) {
            std::sort(result.begin(), result.end(),
                      [history](const WindowGroup& a, const WindowGroup& b) {
                qint64 latestA = 0, latestB = 0;
                for (const auto& win : a.windows)
                    latestA = qMax(latestA, history->latestActivation(win.hwnd));
                for (const auto& win : b.windows)
                    latestB = qMax(latestB, history->latestActivation(win.hwnd));
                return latestA > latestB;
            });
        }

        return result;
    }

} // namespace WindowGrouper
