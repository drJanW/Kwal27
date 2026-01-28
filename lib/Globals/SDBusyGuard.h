#pragma once

#include "SDManager.h"

/**
 * @brief RAII guard for SD card locking
 * 
 * Automatically calls lockSD() on construction and unlockSD() on destruction.
 * Since SD locking is reentrant, this always succeeds.
 */
class SDBusyGuard {
public:
    SDBusyGuard() {
        SDManager::lockSD();
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
            SDManager::unlockSD();
            released_ = true;
        }
    }

private:
    bool released_ = false;
};
