#include "WindowFilter.h"

QList<WindowDescriptor> WindowFilter::filter(const QList<WindowDescriptor>& windows) const {
    QList<WindowDescriptor> result;
    result.reserve(windows.size());
    for (const auto& w : windows) {
        if (shouldInclude(w))
            result.append(w);
    }
    return result;
}

void WindowFilter::setRules(const WindowFilterRule& rules) {
    m_rule = rules;
}

bool WindowFilter::shouldInclude(const WindowDescriptor& desc) const {
    for (const auto& entry : m_rule.entries) {
        bool matches = true;
        if (!entry.title.isEmpty())
            matches = matches && desc.title.contains(entry.title, Qt::CaseInsensitive);
        if (!entry.className.isEmpty())
            matches = matches && desc.className.contains(entry.className, Qt::CaseInsensitive);
        if (!entry.processName.isEmpty())
            matches = matches && desc.processName.contains(entry.processName, Qt::CaseInsensitive);
        if (!entry.processPath.isEmpty())
            matches = matches && desc.processPath.startsWith(entry.processPath, Qt::CaseInsensitive);
        if (matches)
            return false;
    }
    return true;
}
