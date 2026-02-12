/**
 * @file NasBackup.h
 * @brief Push pattern/color CSVs to NAS after save
 * @version 260213B
 * @date 2026-02-13
 */
#pragma once

namespace NasBackup {
    /// Mark a CSV file as pending upload to NAS.
    /// Starts a repeating timer that pushes when safe (WiFi ok, NAS ok, SD free, audio idle).
    void requestPush(const char* filename);

    /// Probe NAS health (GET /api/health). Updates AlertState SC_NAS.
    /// Called from WiFiBoot after connect. On success schedules next check in 57 min.
    /// On failure: fast retries (2 min × 3), then WiFi reconnect + one more try.
    void checkHealth();
}
