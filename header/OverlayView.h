#ifndef WIN_SWITCHER_OVERLAYVIEW_H
#define WIN_SWITCHER_OVERLAYVIEW_H

#include <Windows.h>
#include <QRect>
#include "WindowTypes.h"

struct OverlayLayout {
    // screen coordinates
    QRect widgetRect;

    // widget-local coordinates
    QRect listRect;
};

class OverlayView {
public:
    virtual ~OverlayView() = default;

    virtual void showOverlay() = 0;
    virtual void doHide() = 0;
    virtual void updateGroups(const QList<WindowGroup>& groups) = 0;
    virtual void applyIconLayout(int iconSize, int gridWidth, int gridHeight) = 0;
    virtual void applyLayout(const OverlayLayout& layout) = 0;
    virtual void setCurrentIndex(int index) = 0;
    virtual HWND hWnd() const = 0;
};

#endif //WIN_SWITCHER_OVERLAYVIEW_H
