/**
 * @file AlertBoot.h
 * @brief Alert system one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles alert subsystem initialization during boot. Configures the
 * AlertRun for hardware failure alerts. Part of the Boot→Plan→
 * Policy→Run pattern for failure alert coordination.
 */

#pragma once

class AlertBoot {
public:
    static void configure();
};