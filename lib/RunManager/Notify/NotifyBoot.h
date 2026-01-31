/**
 * @file NotifyBoot.h
 * @brief Notification system one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles notification subsystem initialization during boot. Configures the
 * NotifyRun for hardware failure notification. Part of the Boot→Plan→
 * Policy→Run pattern for failure notification coordination.
 */

#pragma once

class NotifyBoot {
public:
    static void configure();
};
