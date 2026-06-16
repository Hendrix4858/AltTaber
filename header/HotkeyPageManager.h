#ifndef WIN_SWITCHER_HOTKEYPAGEMANAGER_H
#define WIN_SWITCHER_HOTKEYPAGEMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include "core/HotkeyAction.h"

class QStackedWidget;
class QLabel;
class ConfigManager;
class HotkeyRecorder;

class HotkeyPageManager : public QObject {
    Q_OBJECT

public:
    explicit HotkeyPageManager(ConfigManager* config, QObject* parent = nullptr);

    void buildHotkeyPage(QStackedWidget* stackedWidget, QLabel* hotkeyPlaceholder = nullptr);
    void loadBindings();
    void applyBindings();
    void cancelAllRecordings();

    bool handleRecordedKey(quint32 vk, quint32 scan, DWORD flags, Qt::KeyboardModifiers mods);

    ConfigManager* config() const { return m_config; }
    HotkeyBindings currentBindings() const;

signals:
    void bindingsChanged(bool hasSingleLetter);

private:
    void checkLetterJumpConflict();

    ConfigManager* m_config;
    QMap<HotkeyAction, HotkeyRecorder*> m_recorders;
    QLabel* m_showSwitcherWarning = nullptr;
    bool m_resolvingConflict = false;
};

#endif //WIN_SWITCHER_HOTKEYPAGEMANAGER_H
