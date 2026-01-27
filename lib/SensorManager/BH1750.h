/**
 * @file BH1750.h
 * @brief Ambient light sensor driver interface
 * @version 251231E
 * @date 2025-12-31
 *
 * Header file for the BH1750 digital ambient light sensor driver.
 * The BH1750 measures illuminance (light intensity) in lux via I2C interface.
 * Supports multiple measurement modes: continuous high-resolution (1 lx),
 * high-resolution mode 2 (0.5 lx), and low-resolution (4 lx) modes.
 * Used to detect ambient lighting conditions for adaptive system behavior.
 */

#pragma once
#include <Arduino.h>
#include <Wire.h>

// Minimal BH1750 driver (no INT).
// Modes: use Continuous High-Res by default (0x10).
// Lux = raw / 1.2 according to datasheet.

class BH1750 {
public:
    enum : uint8_t {
        ADDR_LOW  = 0x23, // ADDR pin LOW or unconnected
        ADDR_HIGH = 0x5C  // ADDR pin HIGH
    };
    enum : uint8_t {
        POWER_DOWN = 0x00,
        POWER_ON   = 0x01,
        RESET      = 0x07,

        CONT_HIGH_RES  = 0x10, // 1 lx, ~120ms
        CONT_HIGH_RES2 = 0x11, // 0.5 lx, ~120ms
        CONT_LOW_RES   = 0x13  // 4 lx, ~16ms
    };

    BH1750() : _w(nullptr), _addr(ADDR_LOW), _mode(CONT_HIGH_RES), _ok(false) {}

    bool begin(TwoWire& w, uint8_t addr = ADDR_LOW, uint8_t mode = CONT_HIGH_RES) {
        _w = &w;
        _addr = addr;
        _mode = mode;
        // Try power on
        if (!writeByte(POWER_ON)) return _ok = false;
        delay(1);
        // Reset is only valid when powered on
        writeByte(RESET);
        delay(1);
        // Set measurement mode
        _ok = writeByte(_mode);
        return _ok;
    }

    bool reconfigure(uint8_t mode) {
        _mode = mode;
        if (!_w) return false;
        if (!writeByte(POWER_ON)) return false;
        delay(1);
        writeByte(RESET);
        delay(1);
        return writeByte(_mode);
    }

    bool isReady() const { return _ok; }

    // Returns true on success. Writes lux to out parameter.
    bool readLux(float& lux) {
        if (!_w) return false;
        // Request two bytes
        _w->beginTransmission(_addr);
        // no command needed in continuous mode
        int rc = _w->endTransmission();
        if (rc != 0) return false;

        uint32_t t0 = millis();
        int available = 0;
        // Typical conv is 120 ms in high-res. Poll up to ~200 ms.
        while ((available = _w->requestFrom((int)_addr, 2)) < 2) {
            if (millis() - t0 > 220) return false;
            delay(2);
        }
        uint16_t raw = ((uint16_t)_w->read() << 8) | _w->read();
        // 0xFFFF indicates over-range in some docs. Clamp.
        if (raw == 0xFFFF) raw = 0xFFFE;
        lux = raw / 1.2f;
        return true;
    }

    // Quick probe: check if device ACKs.
    static bool probe(TwoWire& w, uint8_t addr) {
        w.beginTransmission(addr);
        return w.endTransmission() == 0;
    }

private:
    bool writeByte(uint8_t v) {
        _w->beginTransmission(_addr);
        _w->write(v);
        return _w->endTransmission() == 0;
    }

    TwoWire* _w;
    uint8_t  _addr;
    uint8_t  _mode;
    bool     _ok;
};
