/**
 * @file SensorsBoot.h
 * @brief Sensor subsystem one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles sensor initialization during boot. Starts sensor status monitoring
 * and reports sensor availability to notification system. Part of the
 * Boot→Plan→Policy→Run pattern for sensor coordination.
 */

#pragma once

class SensorsBoot {
public:
    void plan();
};
