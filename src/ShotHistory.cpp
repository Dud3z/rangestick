#include "ShotHistory.h"
#include <Preferences.h>
#include <cstdio>
#include <cstring>

namespace ShotHistory {

namespace {
void slotKey(char* buf, int slot) {
    snprintf(buf, 8, "s%d", slot);
}
} // namespace

int count() {
    Preferences prefs;
    prefs.begin("history", true);
    uint16_t c = prefs.getUShort("cnt", 0);
    prefs.end();
    if (c > kMaxSessions) c = kMaxSessions;
    return c;
}

void get(int indexFromNewest, Session& out) {
    out = Session();
    Preferences prefs;
    prefs.begin("history", true);
    uint16_t c = prefs.getUShort("cnt", 0);
    uint16_t nextSlot = prefs.getUShort("next", 0);
    if (indexFromNewest < 0 || indexFromNewest >= c) {
        prefs.end();
        return;
    }
    // Newest session sits at (nextSlot - 1), walking backwards through the ring buffer.
    int slot = (static_cast<int>(nextSlot) - 1 - indexFromNewest + kMaxSessions) % kMaxSessions;
    char key[8];
    slotKey(key, slot);
    prefs.getBytes(key, &out, sizeof(Session));
    prefs.end();
}

void add(const Session& session) {
    Preferences prefs;
    prefs.begin("history", false);
    uint16_t c = prefs.getUShort("cnt", 0);
    uint16_t nextSlot = prefs.getUShort("next", 0);

    char key[8];
    slotKey(key, nextSlot);
    prefs.putBytes(key, &session, sizeof(Session));

    nextSlot = (nextSlot + 1) % kMaxSessions;
    if (c < kMaxSessions) ++c;
    prefs.putUShort("cnt", c);
    prefs.putUShort("next", nextSlot);
    prefs.end();
}

void clearAll() {
    Preferences prefs;
    prefs.begin("history", false);
    prefs.clear();
    prefs.end();
}

} // namespace ShotHistory
