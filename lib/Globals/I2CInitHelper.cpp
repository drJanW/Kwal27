/**
 * @file I2CInitHelper.cpp
 * @brief Generic I2C device initialization implementation
 * @version 251231E
 * @date 2025-12-31
 */

#include <Arduino.h>
#include "I2CInitHelper.h"
#include "TimerManager.h"
#include "Globals.h"

#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO

namespace {

constexpr uint8_t MAX_DEVICES = 8;

struct DeviceState {
    I2CInitConfig cfg;
    TimerCallback cb;
    bool ready;
    bool failed;
};

DeviceState devices[MAX_DEVICES];
uint8_t deviceCount = 0;

int findDevice(StatusComponent comp) {
    for (uint8_t i = 0; i < deviceCount; i++) {
        if (devices[i].cfg.comp == comp) return i;
    }
    return -1;
}

} // anonymous namespace

namespace I2CInitHelper {

void start(const I2CInitConfig& cfg, TimerCallback cb) {
    if (deviceCount >= MAX_DEVICES) {
        PF("[I2CInit] Max devices reached, cannot add %s\n", cfg.name);
        return;
    }
    
    // Check if already registered
    int existing = findDevice(cfg.comp);
    if (existing >= 0) {
        PF("[I2CInit] %s already registered\n", cfg.name);
        return;
    }
    
    // Register device
    DeviceState& dev = devices[deviceCount++];
    dev.cfg = cfg;
    dev.cb = cb;
    dev.ready = false;
    dev.failed = false;
    
    PF("[I2CInit] %s starting, max %d retries with %.1fx growth\n", cfg.name, cfg.maxRetries, cfg.growth);
    
    // Start timer with per-device callback
    TimerManager::instance().create(cfg.startDelayMs, cfg.maxRetries, cb, cfg.growth);
}

void tryInit(StatusComponent comp) {
    int idx = findDevice(comp);
    if (idx < 0) return;
    
    DeviceState& dev = devices[idx];
    if (dev.ready || dev.failed) return;  // Guard: already done
    
    // Get remaining retries
    int16_t remaining = TimerManager::instance().getRepeatCount(dev.cb);
    if (remaining != -1) {
        NotifyState::set(dev.cfg.comp, static_cast<uint8_t>(remaining));
    }
    
    // Try probe
    if (dev.cfg.probe()) {
        dev.ready = true;
        TimerManager::instance().cancel(dev.cb);
        NotifyConduct::report(dev.cfg.okIntent);  // report() does setOk()
        PF("[I2CInit] %s ready\n", dev.cfg.name);
        return;
    }
    
    // Check if last retry
    if (remaining == 1) {
        dev.failed = true;
        NotifyConduct::report(dev.cfg.failIntent);  // report() does setOk()
        PF("[I2CInit] %s failed after %d retries\n", dev.cfg.name, dev.cfg.maxRetries);
    }
}

bool isReady(StatusComponent comp) {
    int idx = findDevice(comp);
    return (idx >= 0) ? devices[idx].ready : false;
}

bool isFailed(StatusComponent comp) {
    int idx = findDevice(comp);
    return (idx >= 0) ? devices[idx].failed : false;
}

} // namespace I2CInitHelper
