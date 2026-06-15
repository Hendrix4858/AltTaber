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
        WindowBlockRule e;
        e.className = c;
        rule.rules.append(e);
    }

    // imestatuspop pattern (prefix match)
    {
        WindowBlockRule e;
        e.className = "imestatuspop_classname{";
        rule.rules.append(e);
    }

    // Blocked exe paths
    {
        WindowBlockRule e;
        e.processPath = R"(C:\Windows\System32\wscript.exe)";
        rule.rules.append(e);
    }

    // Blocked file names
    QStringList blockedFileNames = {
        "Nahimic3.exe",
        "Follower.exe",
        "QQ Follower.exe"
    };
    for (const auto& f : blockedFileNames) {
        WindowBlockRule e;
        e.processName = f;
        rule.rules.append(e);
    }

    return rule;
}

QList<WindowDescriptor> WindowFilter::filter(const QList<WindowDescriptor>& windows) const {
    QList<WindowDescriptor> result;
    result.reserve(windows.size());
    for (const auto& w : windows) {
        if (isAllowed(w))
            result.append(w);
    }
    return result;
}

void WindowFilter::setRules(const WindowFilterRule& rules) {
    m_rule = rules;
}

bool WindowFilter::isAllowed(const WindowDescriptor& desc) const {
    for (const auto& rule : m_rule.rules) {
        bool matches = true;
        if (!rule.title.isEmpty())
            matches = matches && desc.title.contains(rule.title, Qt::CaseInsensitive);
        if (!rule.className.isEmpty())
            matches = matches && desc.className.startsWith(rule.className, Qt::CaseInsensitive);
        if (!rule.processName.isEmpty())
            matches = matches && QFileInfo(desc.processPath).fileName().compare(rule.processName, Qt::CaseInsensitive) == 0;
        if (!rule.processPath.isEmpty())
            matches = matches && desc.processPath.startsWith(rule.processPath, Qt::CaseInsensitive);
        if (matches)
            return false;
    }
    return true;
}
