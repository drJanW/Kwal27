/**
 * @file BootManager.h
 * @brief Boot sequence coordinator
 * @version 260304F
 * @date 2026-03-04
 */
#pragma once

class BootManager {
public:
    bool begin();
    void restartBootTimer();  // Call after globals.csv load

private:
    void cb_bootstrap();
    static void cb_bootstrapThunk();
    static void cb_fallbackThunk();
    void cancelFallbackTimer();
    void fallbackTimeout();

    struct FallbackStatus {
        bool seedAttempted = false;
        bool seededFromCache = false;
        bool seededFromRtc = false;
        bool stateAnnounced = false;

        void resetFlags() {
            seedAttempted = false;
            seededFromCache = false;
            seededFromRtc = false;
            stateAnnounced = false;
        }
    };

    FallbackStatus fallback;
};

extern BootManager bootManager;
