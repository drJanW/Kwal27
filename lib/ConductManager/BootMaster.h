/**
 * @file BootMaster.h
 * @brief Master boot sequence orchestrator
 * @version 251231E
 * @date 2025-12-31
 *
 * Orchestrates the complete system boot sequence, coordinating initialization
 * of all subsystems in the correct order. Manages fallback timers to ensure
 * the system reaches a usable state even if some components fail. Tracks boot
 * progress and handles timeout scenarios for clock seeding and system readiness.
 */

#ifndef BOOTMASTER_H
#define BOOTMASTER_H

class BootMaster {
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

extern BootMaster bootMaster;

#endif // BOOTMASTER_H
