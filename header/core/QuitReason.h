#ifndef QUITREASON_H
#define QUITREASON_H

namespace QuitReason {
    inline bool& flag() { static bool f = false; return f; }
    inline void markIntentional() { flag() = true; }
    inline bool isIntentional() { return flag(); }
    inline void reset() { flag() = false; }
}

#endif // QUITREASON_H
