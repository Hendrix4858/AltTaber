#include "WindowManager.h"
#include "WindowEnumerator.h"
#include "WindowGrouper.h"
#include "WindowFilter.h"
#include "core/ConfigManager.h"

WindowManager::WindowManager(ConfigManager* config, HWND selfHwnd, QObject* parent)
    : QObject(parent), m_config(config), m_selfHwnd(selfHwnd) {
    reloadFilterRules();
}

void WindowManager::setSelfHwnd(HWND hwnd) { m_selfHwnd = hwnd; }

QList<WindowGroup> WindowManager::prepareWindowGroupList() {
    auto scope = static_cast<int>(m_config->getVirtualDesktopScope());
    auto descriptors = WindowEnumerator::enumValidWindows(scope);
    auto filtered = m_filter.filter(descriptors);
    return WindowGrouper::groupWindows(filtered, &m_activationHistory, m_selfHwnd);
}

void WindowManager::recordWindowActivation(const AppIdentity& identity) {
    m_activationHistory.record(identity);
}

void WindowManager::reloadFilterRules() {
    auto rule = WindowFilter::buildRuleFromConfig(*m_config);
    m_filter.setRules(rule);
    qInfo() << "[Filter] reloaded" << rule.rules.size() << "rules"
            << "(builtin:" << WindowFilter::builtinRules().rules.size() << ")";
}

QList<HWND> WindowManager::filteredHwndsForExe(const QString& exePath) {
    auto scope = static_cast<int>(m_config->getVirtualDesktopScope());
    auto descriptors = WindowEnumerator::enumValidWindows(scope);
    QList<WindowDescriptor> filteredByExe;
    for (const auto& desc : descriptors) {
        if (desc.processPath.compare(exePath, Qt::CaseInsensitive) == 0)
            filteredByExe.append(desc);
    }
    auto filtered = m_filter.filter(filteredByExe);
    QList<HWND> hwnds;
    hwnds.reserve(filtered.size());
    for (const auto& d : filtered)
        hwnds.append(d.hwnd);
    return hwnds;
}
