#pragma once

#include <cstdint>

// Persisted log of recent shot-timer sessions, own Preferences namespace ("history") --
// separate from AppSettings/WifiConfig. Stored as a ring buffer of up to kMaxSessions entries;
// once full, adding a new session overwrites the oldest one.
namespace ShotHistory {

constexpr int kMaxSessions = 50;
constexpr int kMaxShotsPerSession = 20; // mirrors ShotTimer::MAX_SHOTS

struct Session {
    uint8_t shotCount = 0;
    uint8_t detectMode = 0; // 0 = microphone, 1 = recoil
    uint32_t shotTimesMs[kMaxShotsPerSession] = {0};
};

int count(); // how many sessions are stored, 0..kMaxSessions
// indexFromNewest: 0 = most recently completed session, 1 = the one before that, etc.
void get(int indexFromNewest, Session& out);
void add(const Session& session);
void clearAll();

} // namespace ShotHistory
