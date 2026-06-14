#include "WindowFilter.h"
#include <QFileInfo>

WindowFilterRule WindowFilter::builtinRules() {
    WindowFilterRule rule;

    // Blocked class names (moved from WindowEnumerator::isWindowAcceptable)
    QStringList blockedClassNames = {
        "Progman",
        "Windows.UI.Core.CoreWindow",
        "CEF-OSC-WIDGET",
        "WorkerW",
        "Shell_TrayWnd"
    };
    for (const auto& c : blockedClassNames) {
        WindowFilterEntry e;
        e.className = c;
        rule.entries.append(e);
    }

    // imestatuspop pattern (prefix match)
    {
        WindowFilterEntry e;
        e.className = "imestatuspop_classname{";
        rule.entries.append(e);
    }

    // Blocked exe paths
    {
        WindowFilterEntry e;
        e.processPath = R"(C:\Windows\System32\wscript.exe)";
        rule.entries.append(e);
    }

    // Blocked file names
    QStringList blockedFileNames = {
        "Nahimic3.exe",
        "Follower.exe",
        "QQ Follower.exe"
    };
    for (const auto& f : blockedFileNames) {
        WindowFilterEntry e;
        e.processName = f;
        rule.entries.append(e);
    }

    return rule;
}

bool WindowFilter::isAllowed(const WindowDescriptor& desc) const {
    return shouldInclude(desc);
}

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
            matches = matches && desc.className.startsWith(entry.className, Qt::CaseInsensitive);
        if (!entry.processName.isEmpty())
            matches = matches && QFileInfo(desc.processPath).fileName().compare(entry.processName, Qt::CaseInsensitive) == 0;
        if (!entry.processPath.isEmpty())
            matches = matches && desc.processPath.startsWith(entry.processPath, Qt::CaseInsensitive);
        if (matches)
            return false;
    }
    return true;
}
