/**
 * @file VL53L1X.cpp
 * @brief Time-of-flight distance sensor driver implementation
 * @version 260215B
 * @date 2026-02-15
 */
#include <Arduino.h>
#include "VL53L1X.h"
#include "Globals.h"

#ifndef VL53L1X_DEBUG
#define VL53L1X_DEBUG 0
#endif

#if VL53L1X_DEBUG
    #define VL53_DBG(...) PF(__VA_ARGS__)
#else
    #define VL53_DBG(...) do {} while (0)
#endif

static SFEVL53L1X l1x;
static bool l1x_inited = false;
static uint16_t l1x_budget = 50;

static void logI2CProbe(uint8_t address, TwoWire &bus) {
    bus.beginTransmission(address);
    uint8_t err = bus.endTransmission();
    if (err == 0) {
        VL53_DBG("[SensorController] I2C address 0x%02X acknowledged\n", address);
    } else {
        VL53_DBG("[SensorController] I2C address 0x%02X no response (err=%u)\n", address, err);
    }
}

bool VL53L1X_begin(uint8_t address, TwoWire &bus, uint16_t timingBudgetMs, bool longRange)
{
    if (!l1x_inited)
    {
        logI2CProbe(address, bus);

        if (!l1x.checkBootState()) {
            VL53_DBG("[SensorController] VL53L1X not ready (boot state false)\n");
            return false;
        }

        uint8_t initStatus = l1x.begin(bus);
        if (initStatus != 0)
        {
            uint16_t id = l1x.getSensorID();
            VL53_DBG("[SensorController] VL53L1X begin() failed at addr 0x%02X (ID=0x%04X status=%u)\n",
                     address, id, initStatus);
            return false;
        }
        l1x_inited = true;
        VL53_DBG("[SensorController] VL53L1X initialized (ID=0x%04X)\n", l1x.getSensorID());
    }
    l1x_budget = timingBudgetMs ? timingBudgetMs : 50;
    if (longRange)
        l1x.setDistanceModeLong();
    else
        l1x.setDistanceModeShort();
    l1x.setTimingBudgetInMs(l1x_budget);
    l1x.startRanging();
    return true;
}

float readVL53L1X()
{
    if (!l1x_inited)
    {
        if (!VL53L1X_begin(VL53L1X_I2C_ADDR, Wire, 50, false))
            return NAN;
    }
    if (!l1x.checkForDataReady())
        return NAN;
    uint16_t mm = l1x.getDistance();
    l1x.clearInterrupt();
    return static_cast<float>(mm);
}
