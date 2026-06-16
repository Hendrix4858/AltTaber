#include "TaskbarMouseHelper.h"
#include <QTimer>

namespace TaskbarMouseHelper {

void click() {
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

void hold() {
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
}

void release() {
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

QTimer* createDelayedRelease(QObject* parent, int holdMs) {
    auto* timer = new QTimer(parent);
    timer->setSingleShot(true);
    timer->setInterval(holdMs);
    QObject::connect(timer, &QTimer::timeout, [timer]() {
        release();
    });
    return timer;
}

} // namespace TaskbarMouseHelper
