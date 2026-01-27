/**
 * @file HeartbeatConduct.h
 * @brief Heartbeat LED state management
 * @version 251231E
 * @date 2025-12-31
 *
 * Manages heartbeat LED blinking state. Controls blink rate based on system
 * state and sensor distance, provides error signaling pattern, and coordinates
 * with HeartbeatPolicy for rate calculations.
 */

#pragma once

#include <Arduino.h>

class HeartbeatConduct {
public:
	void plan();
	void setRate(uint32_t intervalMs);
	uint32_t currentRate() const;
	void signalError();   // Fast blink pattern for errors

private:
	uint32_t _savedRate = 0;
};

extern HeartbeatConduct heartbeatConduct;
