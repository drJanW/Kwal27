/**
 * @file NasBackup.h
 * @brief Push pattern/color CSVs to NAS after save
 * @version 260210J
 * @date 2026-02-10
 */
#pragma once
#include <Arduino.h>

namespace NasBackup {
    /// Request deferred push of a CSV file to NAS.
    /// File is read from SD and POSTed to csv_server.py after a short delay.
    /// Safe to call from web request handlers (non-blocking).
    void requestPush(const char* filename);
}
