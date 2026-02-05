/**
 * @file HeartbeatRun.h
 * @brief Heartbeat LED state management
 * @version 260131A
 * @date 2026-01-31
 */
#pragma once

#include <Arduino.h>

class HeartbeatRun {
public:
	void plan();
	void setRate(uint32_t intervalMs);
	uint32_t currentRate() const;
	void signalError();   // Fast blink pattern for errors

private:
	uint32_t _savedRate = 0;
};

extern HeartbeatRun heartbeatRun;
