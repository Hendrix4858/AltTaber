#include "WindowManager.h"
#include "utils/Util.h"
#include <QDateTime>
#include <QMap>
#include <algorithm>

WindowManager::WindowManager(HWND selfHwnd, QObject* parent)
    : QObject(parent), m_selfHwnd(selfHwnd) {}

void WindowManager::setSelfHwnd(HWND hwnd) { m_selfHwnd = hwnd; }

QList<WindowGroup> WindowManager::prepareWindowGroupList() {
    QMap<QString, WindowGroup> winGroupMap;
    const auto list = Util::listValidWindows();
    for (auto hwnd : list) {
        if (hwnd == m_selfHwnd) continue;
        auto path = Util::getWindowProcessPath(hwnd);
        if (path.isEmpty()) continue;
        auto& winGroup = winGroupMap[path];
        if (winGroup.exePath.isEmpty()) {
            winGroup.exePath = path;
            auto icon = Util::getCachedIcon(path, hwnd);
            if (path.endsWith("QQ\\bin\\QQ.exe", Qt::CaseInsensitive)) {
                QPixmap overlay = Util::getWindowIcon(hwnd);
                QPixmap bgPixmap = icon.pixmap(64, 64);
                icon = Util::overlayIcon(bgPixmap, overlay, {QPoint(32, 32), QSize(32, 32)});
            }
            winGroup.icon = icon;
        }
        winGroup.addWindow({Util::getWindowTitle(hwnd), Util::getClassName(hwnd), hwnd});
    }
    auto winGroupList = winGroupMap.values();
    std::sort(winGroupList.begin(), winGroupList.end(),
              [this](const WindowGroup& a, const WindowGroup& b) {
                  auto timeA = getLastValidActiveGroupWindow(a).second;
                  auto timeB = getLastValidActiveGroupWindow(b).second;
                  if (timeA.isNull() && timeB.isNull()) return false;
                  if (timeA.isValid() && timeB.isValid()) return timeA > timeB;
                  return timeA.isValid();
              });

    cleanupStaleEntries();
    return winGroupList;
}

void WindowManager::notifyWindowActivated(HWND hwnd, const QString& exePath) {
    m_winActiveOrder[exePath].insert(hwnd, QDateTime::currentDateTime());
}

void WindowManager::cleanupStaleEntries() {
    for (auto it = m_winActiveOrder.begin(); it != m_winActiveOrder.end();) {
        auto& hwndMap = it.value();
        for (auto hwndIt = hwndMap.begin(); hwndIt != hwndMap.end();) {
            if (!IsWindow(hwndIt.key()))
                hwndIt = hwndMap.erase(hwndIt);
            else
                ++hwndIt;
        }
        if (hwndMap.isEmpty())
            it = m_winActiveOrder.erase(it);
        else
            ++it;
    }
}

QPair<HWND, QDateTime> WindowManager::getLastActiveGroupWindow(const QString& exePath) const {
    auto hwndOrder = m_winActiveOrder.value(exePath);
    if (hwndOrder.isEmpty()) return {nullptr, QDateTime()};
    auto iter = std::max_element(hwndOrder.begin(), hwndOrder.end());
    return {iter.key(), iter.value()};
}

QPair<HWND, QDateTime> WindowManager::getLastValidActiveGroupWindow(const WindowGroup& group) const {
    auto hwndOrder = m_winActiveOrder.value(group.exePath);
    if (hwndOrder.isEmpty()) return {nullptr, QDateTime()};

    QList<HWND> windows;
    for (auto& info : group.windows)
        windows << info.hwnd;
    sortGroupWindows(windows, group.exePath);

    if (auto time = hwndOrder.value(windows.first()); !time.isNull())
        return {windows.first(), time};
    else
        return {nullptr, QDateTime()};
}

void WindowManager::sortGroupWindows(QList<HWND>& windows, const QString& exePath) const {
    auto activeOrdMap = m_winActiveOrder.value(exePath);
    if (activeOrdMap.isEmpty()) return;
    std::sort(windows.begin(), windows.end(),
              [&activeOrdMap](HWND a, HWND b) {
                  return activeOrdMap.value(a) > activeOrdMap.value(b);
              });
}

QList<HWND> WindowManager::buildGroupWindowOrder(const QString& exePath) const {
    auto windows = Util::listValidWindows(exePath);
    sortGroupWindows(windows, exePath);
    return windows;
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
