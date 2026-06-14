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

bool WheelEventProcessor::handleWheelEvent(QWheelEvent* event, QListView* lv,
                                            WindowGroupModel* model, WindowManager* wm,
                                            GroupWindowCycler* cyc) {
    auto cursorPos = event->position().toPoint();
    if (auto index = lv->indexAt(cursorPos); index.isValid()) {
        if (lv->currentIndex() != index)
            lv->setCurrentIndex(index);
        auto windowGroup = model->groupAt(index.row());
        if (windowGroup.windows.isEmpty()) return false;

        if (m_lastRow != index.row()) {
            m_lastRow = index.row();
            m_lastHwnd = nullptr;
            cyc->clearGroupWindowOrder();
        }
        auto targetExe = windowGroup.exePath;
        bool isRollUp = event->angleDelta().x() > 0;
        auto& order = cyc->groupWindowOrder();
        if (order.isEmpty())
            order = wm->filteredHwndsForExe(targetExe);

        if (!m_lastHwnd) {
            m_lastHwnd = order.first();
        } else {
            if (m_lastIsRollUp == isRollUp)
                m_lastHwnd = GroupWindowCycler::rotateWindowInGroup(order, m_lastHwnd, isRollUp);
        }
        m_lastIsRollUp = isRollUp;

        HWND nextFocus = m_lastHwnd;
        if (isRollUp) {
            Util::bringWindowToTop(m_lastHwnd, nullptr);
        } else {
            auto& orderForNormal = cyc->groupWindowOrder();
            if (auto normal = GroupWindowCycler::rotateNormalWindowInGroup(orderForNormal, m_lastHwnd, false)) {
                ShowWindow(normal, SW_SHOWMINNOACTIVE);
                m_lastHwnd = normal;
                nextFocus = m_lastHwnd;
            }
            if (auto normal = GroupWindowCycler::rotateNormalWindowInGroup(orderForNormal, m_lastHwnd, false))
                nextFocus = normal;
        }
        emit foregroundChanged(nextFocus);
        emit labelTextChanged(Util::getWindowTitle(nextFocus));
        return true;
    }
    return false;
}
