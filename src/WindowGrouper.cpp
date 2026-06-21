#include "WindowGrouper.h"
#include "ActivationHistory.h"
#include "utils/Util.h"
#include "utils/PwaDetector.h"
#include "core/ConfigManager.h"
#include <algorithm>

namespace WindowGrouper {

    namespace {

        void populateFileDescriptionTokens(WindowGroup& group) {
            QString name = Util::getFileDescription(group.exePath);
            if (name.isEmpty()) return;
            QChar fl = Util::getDisplayFirstLetter(name);
            if (!fl.isNull())
                group.fileDescriptionTokens.insert(fl);
        }

        void populatePwaTokens(WindowGroup& group) {
            for (const auto& win : group.windows) {
                if (win.pwaDisplayName.isEmpty()) continue;
                QChar fl = Util::getDisplayFirstLetter(win.pwaDisplayName);
                if (!fl.isNull())
                    group.pwaNameTokens.insert(fl);
            }
        }

        void populateTitleTokens(WindowGroup& group) {
            for (const auto& win : group.windows) {
                if (win.title.isEmpty()) continue;
                QChar fl = Util::getDisplayFirstLetter(win.title);
                if (!fl.isNull())
                    group.titleTokens.insert(fl);
            }
        }

        void populateDisplayNameTokens(WindowGroup& group) {
            if (group.displayName.isEmpty()) return;
            QChar fl = Util::getDisplayFirstLetter(group.displayName);
            if (!fl.isNull())
                group.displayNameTokens.insert(fl);
        }

        void populateProcessNameTokens(WindowGroup&) {
            // Reserved for future use
        }

        void populateJumpTokens(WindowGroup& group) {
            populateFileDescriptionTokens(group);
            populatePwaTokens(group);
            populateTitleTokens(group);
            populateDisplayNameTokens(group);
            populateProcessNameTokens(group);

            group.allJumpTokens = group.displayNameTokens
                                | group.pwaNameTokens
                                | group.titleTokens;
        }

    } // anonymous namespace

    QList<WindowGroup> groupWindows(const QList<WindowDescriptor>& windows,
                                    const ActivationHistory* history,
                                    HWND selfHwnd) {
        QList<WindowGroup> result;
        QMap<QString, int> indexMap;

        bool pwaEnabled = cfg().getPwaEnabled();
        bool separateGroups = pwaEnabled && cfg().getPwaMode() == PwaMode::SeparateGroups;

        for (const auto& desc : windows) {
            if (desc.hwnd == selfHwnd) continue;
            if (desc.processPath.isEmpty()) continue;

            QString groupKey = desc.processPath;
            if (separateGroups && desc.windowKind == WindowKind::Pwa)
                groupKey = desc.appUserModelId;

            WindowInfo winInfo{desc.title, desc.className, desc.appUserModelId,
                               desc.hwnd, desc.windowKind, desc.pwaDisplayName};

            if (auto it = indexMap.find(groupKey); it != indexMap.end()) {
                result[it.value()].addWindow(winInfo);
            } else {
                WindowGroup group;
                group.exePath = desc.processPath;
                if (desc.windowKind == WindowKind::Pwa && separateGroups) {
                    group.icon = PwaDetector::getPwaIcon(desc.hwnd, desc.appUserModelId, desc.processPath);
                    group.displayName = desc.pwaDisplayName;
                } else {
                    group.icon = Util::getCachedIcon(desc.processPath, desc.hwnd);
                }
                if (group.displayName.isEmpty())
                    group.displayName = Util::getFileDescription(desc.processPath);

                group.addWindow(winInfo);
                populateJumpTokens(group);
                indexMap[groupKey] = result.size();
                result.append(group);
            }
        }

        if (history) {
            std::sort(result.begin(), result.end(),
                      [history](const WindowGroup& a, const WindowGroup& b) {
                qint64 latestA = 0, latestB = 0;
                for (const auto& win : a.windows)
                    latestA = qMax(latestA, history->lastActivationTime(win.hwnd));
                for (const auto& win : b.windows)
                    latestB = qMax(latestB, history->lastActivationTime(win.hwnd));
                return latestA > latestB;
            });
        }

        return result;
    }

} // namespace WindowGrouper
