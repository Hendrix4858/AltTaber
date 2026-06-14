#ifndef WIN_SWITCHER_WINDOWFILTER_H
#define WIN_SWITCHER_WINDOWFILTER_H

#include <QString>
#include <QList>
#include "WindowTypes.h"

struct WindowFilterEntry {
    QString title;
    QString className;
    QString processName;
    QString processPath;
};

struct WindowFilterRule {
    QList<WindowFilterEntry> entries;
};

class WindowFilter {
public:
    QList<WindowDescriptor> filter(const QList<WindowDescriptor>& windows) const;
    void setRules(const WindowFilterRule& rules);
    const WindowFilterRule& rules() const { return m_rule; }

    static WindowFilterRule builtinRules();
    bool isAllowed(const WindowDescriptor& desc) const;

private:
    bool shouldInclude(const WindowDescriptor& desc) const;

    WindowFilterRule m_rule;
};

#endif //WIN_SWITCHER_WINDOWFILTER_H
