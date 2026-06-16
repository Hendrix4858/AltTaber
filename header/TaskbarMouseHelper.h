#ifndef WIN_SWITCHER_TASKBARMOUSEHELPER_H
#define WIN_SWITCHER_TASKBARMOUSEHELPER_H

#include <Windows.h>
#include <QObject>

class QTimer;

namespace TaskbarMouseHelper {

void click();
void hold();
void release();

QTimer* createDelayedRelease(QObject* parent, int holdMs = 200);

} // namespace TaskbarMouseHelper

#endif //WIN_SWITCHER_TASKBARMOUSEHELPER_H
