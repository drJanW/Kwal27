/**
 * @file I2CInitHelper.cpp
 * @brief Generic I2C device initialization implementation
 * @version 260206A
 * @date 2026-02-06
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
    
    PF_BOOT("[I2CInit] %s starting, max %d retries\n", cfg.name, cfg.maxRetries);
    
    // Start timer with per-device callback
    timers.create(cfg.startDelayMs, cfg.maxRetries, cb, cfg.growth);
}

void tryInit(StatusComponent comp) {
    int idx = findDevice(comp);
    if (idx < 0) return;
    
    DeviceState& dev = devices[idx];
    if (dev.ready || dev.failed) return;  // Guard: already done
    
    // Get remaining retries
    auto remaining = timers.remaining();
    AlertState::set(dev.cfg.comp, remaining);
    
    // Try probe
    if (dev.cfg.probe()) {
        dev.ready = true;
        timers.cancel(dev.cb);
        AlertRun::report(dev.cfg.okRequest);
        PF_BOOT("[I2CInit] %s ready\n", dev.cfg.name);
        return;
    }
    
    // Check if last retry
    if (remaining == 1) {
        dev.failed = true;
        AlertRun::report(dev.cfg.failRequest);  // report() does setOk()
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
