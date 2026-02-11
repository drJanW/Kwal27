/**
 * @file NasBackup.h
 * @brief Push pattern/color CSVs to NAS after save
 * @version 260211A
 * @date 2026-02-11
 */
#pragma once

namespace NasBackup {
    /// Mark a CSV file as pending upload to NAS.
    /// Starts a repeating timer that pushes when safe (WiFi ok, NAS ok, SD free, audio idle).
    void requestPush(const char* filename);

    /// Probe NAS health (GET /api/health). Updates AlertState SC_NAS.
    /// Called from WiFiBoot after connect; also runs on a 57-minute repeating timer.
    void checkHealth();
}
