// Stubs for externals used by OverlayController in test context.
// Provides non-inline implementations AND explicit MOC includes.
// This avoids linking the entire dependency tree (WindowUtil, SystemTray, etc.).

#include "OverlayView.h"
#include "WindowGroupModel.h"
#include "WindowManager.h"
#include "core/ConfigManagerBase.h"
#include "core/ThemeManager.h"
// Qt test pattern: make private -> public so we can construct the singleton
#define private public
#include "lifecycle/SystemTray.h"
#undef private
#include "utils/Util.h"
#include <QApplication>

// ── Util stubs ──

namespace Util {
    QString getWindowTitle(HWND) { return {}; }
    QString getClassName(HWND) { return {}; }
    QString getFileDescription(const QString&) { return {}; }
    QString getWindowProcessPath(HWND) { return {}; }
    bool isWindowAllowed(HWND, bool) { return true; }
    void closeSystemWindows() {}
}

// ── SystemTray stub ──

SystemTray::SystemTray(QWidget*) : QSystemTrayIcon(nullptr) {}

SystemTray& SystemTray::instance() {
    static SystemTray s_stub;
    return s_stub;
}

// ── WindowManager stub (non-inline methods) ──

WindowManager::WindowManager(ConfigManager*, HWND, QObject* parent) : QObject(parent) {}
void WindowManager::setSelfHwnd(HWND) {}
QList<WindowGroup> WindowManager::prepareWindowGroupList() { return {}; }
void WindowManager::recordWindowActivation(HWND) {}
void WindowManager::reloadFilterRules() {}
QList<HWND> WindowManager::filteredHwndsForExe(const QString&) { return {}; }

// ── WindowGroupModel stub ──

WindowGroupModel::WindowGroupModel(QObject* parent) : QAbstractListModel(parent) {}
int WindowGroupModel::rowCount(const QModelIndex&) const { return m_groups.size(); }
QVariant WindowGroupModel::data(const QModelIndex&, int) const { return {}; }
void WindowGroupModel::setGroups(const QList<WindowGroup>& g) { m_groups = g; }
void WindowGroupModel::setGroupIcon(int, const QIcon&) {}
const WindowGroup& WindowGroupModel::groupAt(int row) const { return m_groups[row]; }

// ── ConfigManagerBase stubs ──

ConfigManagerBase::ConfigManagerBase(const QString&) : QObject(nullptr) {}
QVariant ConfigManagerBase::get(const QString&, const QVariant&) const { return {}; }
void ConfigManagerBase::set(const QString&, const QVariant&) {}
void ConfigManagerBase::remove(const QString&) {}
void ConfigManagerBase::sync() {}

// ── ThemeManager stubs ──

static ThemeColors s_defaultThemeColors{};
const ThemeColors& ThemeManager::current() { return s_defaultThemeColors; }


