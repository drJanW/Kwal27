#pragma once

/**
 * @file SDBusyGuard.h
 * @brief RAII guard for SD card locking
 * @version 260227B
 * @date 2026-02-27
 */
#include "SdFileAccess.h"

/**
 * @brief RAII guard for SD card locking
 * 
 * Automatically calls lock() on construction and unlock() on destruction.
 * Since SD locking is reentrant, this always succeeds.
 */
class SDBusyGuard {
public:
    SDBusyGuard() {
        SdFileAccess::lock();
    }

    ~SDBusyGuard() {
        release();
    }

    // Always returns true (reentrant lock always succeeds)
    bool acquired() const {
        return !released_;
    }

    void release() {
        if (!released_) {
            SdFileAccess::unlock();
            released_ = true;
        }
    }

private:
    bool released_ = false;
};
