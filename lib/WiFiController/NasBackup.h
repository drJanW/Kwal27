/**
 * @file NasBackup.h
 * @brief Push pattern/color CSVs to NAS after save
 * @version 260220B
 * @date 2026-02-20
 */
#pragma once

namespace NasBackup {
    /// Mark a CSV file as pending upload to NAS.
    /// Starts a repeating timer that pushes when safe (WiFi ok, NAS ok, SD free, audio idle).
    void requestPush(const char* filename);

    /// Probe NAS health (GET /api/health). Updates AlertState SC_NAS.
    void checkHealth();

    /// Start infinite repeating health check timer (every 2 min). Call once from WiFiBoot.
    void startHealthTimer();
}
