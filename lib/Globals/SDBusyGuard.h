#pragma once

/**
 * @file SDBusyGuard.h
 * @brief RAII guard for SD card locking
 * @version 260201A
 * @date 2026-02-01
 */
#include "SDController.h"

/**
 * @brief RAII guard for SD card locking
 * 
 * Automatically calls lockSD() on construction and unlockSD() on destruction.
 * Since SD locking is reentrant, this always succeeds.
 */
class SDBusyGuard {
public:
    SDBusyGuard() {
        SDController::lockSD();
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
            SDController::unlockSD();
            released_ = true;
        }
    }

private:
    bool released_ = false;
};
