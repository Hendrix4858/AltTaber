#include "WindowManager.h"
#include "WindowEnumerator.h"
#include "WindowGrouper.h"
#include "WindowFilter.h"
#include "core/ConfigManager.h"

WindowManager::WindowManager(HWND selfHwnd, QObject* parent)
    : QObject(parent), m_selfHwnd(selfHwnd) {
    reloadFilterRules();
}

void WindowManager::setSelfHwnd(HWND hwnd) { m_selfHwnd = hwnd; }

QList<WindowGroup> WindowManager::prepareWindowGroupList() {
    auto descriptors = WindowEnumerator::enumValidWindows();
    auto filtered = m_filter.filter(descriptors);
    return WindowGrouper::groupWindows(filtered, &m_activationHistory, m_selfHwnd);
}

void WindowManager::recordWindowActivation(HWND hwnd) {
    m_activationHistory.record(hwnd);
}

void WindowManager::reloadFilterRules() {
    WindowFilterRule rule = WindowFilter::builtinRules();
    auto blocked = cfg().getBlockedWindows();
    for (const auto& entry : blocked) {
        if (!entry.enabled) continue;
        WindowBlockRule blockRule;
        blockRule.title = entry.title;
        blockRule.className = entry.className;
        blockRule.processName = entry.processName;
        blockRule.processPath = entry.processPath;
        rule.rules.append(blockRule);
    }
    m_filter.setRules(rule);
    qInfo() << "[Filter] reloaded" << rule.rules.size() << "rules"
            << "(builtin:" << WindowFilter::builtinRules().rules.size() << ")";
}

QList<HWND> WindowManager::filteredHwndsForExe(const QString& exePath) {
    auto descriptors = WindowEnumerator::enumValidWindows(exePath);
    auto filtered = m_filter.filter(descriptors);
    QList<HWND> hwnds;
    hwnds.reserve(filtered.size());
    for (const auto& d : filtered)
        hwnds.append(d.hwnd);
    return hwnds;
}
