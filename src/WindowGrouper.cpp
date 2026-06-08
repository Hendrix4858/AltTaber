#include "WindowGrouper.h"
#include "ActivationHistory.h"
#include "utils/Util.h"
#include <algorithm>

namespace WindowGrouper {

    QList<WindowGroup> groupWindows(const QList<HWND>& hwnds,
                                    const ActivationHistory* history,
                                    HWND selfHwnd) {
        QList<WindowGroup> result;
        QMap<QString, int> pathIndex;

        for (auto hwnd : hwnds) {
            if (hwnd == selfHwnd) continue;
            auto path = Util::getWindowProcessPath(hwnd);
            if (path.isEmpty()) continue;

            if (auto it = pathIndex.find(path); it != pathIndex.end()) {
                result[it.value()].addWindow(
                    {Util::getWindowTitle(hwnd), Util::getClassName(hwnd), hwnd});
            } else {
                WindowGroup group;
                group.exePath = path;
                group.icon = Util::getCachedIcon(path, hwnd);
                group.addWindow(
                    {Util::getWindowTitle(hwnd), Util::getClassName(hwnd), hwnd});
                pathIndex[path] = result.size();
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
