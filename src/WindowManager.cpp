#include "WindowManager.h"
#include "utils/Util.h"
#include <QMap>
#include <QDateTime>
#include <algorithm>

WindowManager::WindowManager(HWND selfHwnd, QObject* parent)
    : QObject(parent), m_selfHwnd(selfHwnd) {}

void WindowManager::setSelfHwnd(HWND hwnd) { m_selfHwnd = hwnd; }

QList<WindowGroup> WindowManager::prepareWindowGroupList() {
    QList<WindowGroup> result;
    QMap<QString, int> pathIndex;
    const auto list = Util::listValidWindows();

    for (auto hwnd : list) {
        if (hwnd == m_selfHwnd) continue;
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
    // Sort by MRU (most recently activated window first)
    std::sort(result.begin(), result.end(), [&](const WindowGroup& a, const WindowGroup& b) {
        auto getLatestActivation = [&](const WindowGroup& g) -> qint64 {
            qint64 latest = 0;
            for (const auto& win : g.windows) {
                auto it = m_windowActivationTimes.find(win.hwnd);
                if (it != m_windowActivationTimes.end())
                    latest = qMax(latest, it.value());
            }
            return latest;
        };
        return getLatestActivation(a) > getLatestActivation(b);
    });

    return result;
}

void WindowManager::recordWindowActivation(HWND hwnd) {
    m_windowActivationTimes[hwnd] = QDateTime::currentMSecsSinceEpoch();
}

QList<HWND> WindowManager::buildGroupWindowOrder(const QString& exePath) const {
    return Util::listValidWindows(exePath);
}

HWND WindowManager::rotateWindowInGroup(const QList<HWND>& windows, HWND current, bool forward) {
    const auto N = windows.size();
    if (N == 1) return windows.first();
    for (int i = 0; i < N; i++) {
        if (windows.at(i) == current) {
            auto next_i = forward ? (i + 1) : (i - 1);
            return windows.at((next_i + N) % N);
        }
    }
    return nullptr;
}

HWND WindowManager::rotateNormalWindowInGroup(const QList<HWND>& windows, HWND current, bool forward) {
    for (int i = 0; IsIconic(current) && i < windows.size(); i++)
        current = rotateWindowInGroup(windows, current, forward);
    return IsIconic(current) ? nullptr : current;
}

QList<HWND>& WindowManager::groupWindowOrder() {
    return m_groupWindowOrder;
}

void WindowManager::clearGroupWindowOrder() {
    m_groupWindowOrder.clear();
}
