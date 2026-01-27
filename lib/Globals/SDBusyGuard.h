#pragma once

#include "SDManager.h"

class SDBusyGuard {
public:
    SDBusyGuard() : ownsLock(!SDManager::isSDbusy()) {
        if (ownsLock) {
            SDManager::setSDbusy(true);
        }
    }

    ~SDBusyGuard() {
        release();
    }

    bool acquired() const {
        return ownsLock;
    }

    void release() {
        if (ownsLock) {
            SDManager::setSDbusy(false);
            ownsLock = false;
        }
    }

private:
    bool ownsLock = false;
};
