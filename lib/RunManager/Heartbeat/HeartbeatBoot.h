/**
 * @file HeartbeatBoot.h
 * @brief Heartbeat LED one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles heartbeat LED initialization during boot. Configures the status LED
 * GPIO pin for heartbeat indication. Part of the Boot→Plan→Policy→Run
 * pattern for periodic debugging feedback.
 */

#pragma once

class HeartbeatBoot {
public:
	void plan();
};

extern HeartbeatBoot heartbeatBoot;
