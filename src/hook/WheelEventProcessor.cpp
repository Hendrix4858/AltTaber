#include "hook/WheelEventProcessor.h"
#include "WindowGroupModel.h"
#include "WindowManager.h"
#include "GroupWindowCycler.h"
#include "utils/Util.h"
#include <QWheelEvent>
#include <QListView>

WheelEventProcessor::WheelEventProcessor(QObject* parent)
    : QObject(parent) {}

void WheelEventProcessor::reset() {
    m_lastRow = -1;
    m_lastHwnd = nullptr;
}

bool WheelEventProcessor::handleWheelEvent(QWheelEvent* event, QListView* listView,
                                            WindowGroupModel* model, WindowManager* wm,
                                            GroupWindowCycler* cyc) {
    auto cursorPos = event->position().toPoint();
    if (auto index = listView->indexAt(cursorPos); index.isValid()) {
        if (listView->currentIndex() != index)
            listView->setCurrentIndex(index);
        auto windowGroup = model->groupAt(index.row());
        if (windowGroup.windows.isEmpty()) return false;

        if (m_lastRow != index.row()) {
            m_lastRow = index.row();
            m_lastHwnd = nullptr;
            cyc->clearGroupWindowOrder();
        }
        auto targetExePath = windowGroup.exePath;
        bool isScrollForward = event->angleDelta().x() > 0;
        auto& order = cyc->groupWindowOrder();
        if (order.isEmpty())
            order = wm->filteredHwndsForExe(targetExePath);

        if (!m_lastHwnd) {
            m_lastHwnd = order.first();
        } else {
            if (m_lastScrollDirection == isScrollForward)
                m_lastHwnd = GroupWindowCycler::rotateWindow(order, m_lastHwnd, isScrollForward);
        }
        m_lastScrollDirection = isScrollForward;

        HWND nextFocus = m_lastHwnd;
        if (isScrollForward) {
            Util::focusWindow(m_lastHwnd, nullptr);
        } else {
            auto& orderForNormal = cyc->groupWindowOrder();
            if (auto normal = GroupWindowCycler::rotateToNormalWindow(orderForNormal, m_lastHwnd, false)) {
                ShowWindow(normal, SW_SHOWMINNOACTIVE);
                m_lastHwnd = normal;
                nextFocus = m_lastHwnd;
            }
            if (auto normal = GroupWindowCycler::rotateToNormalWindow(orderForNormal, m_lastHwnd, false))
                nextFocus = normal;
        }
        emit foregroundChanged(nextFocus);
        emit labelTextChanged(Util::getWindowTitle(nextFocus));
        return true;
    }
    return false;
}
