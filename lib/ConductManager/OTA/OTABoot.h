/**
 * @file OTABoot.h
 * @brief OTA update one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles OTA subsystem initialization during boot. Configures OTA policy
 * settings. Part of the Boot→Plan→Policy→Conduct pattern for OTA update
 * coordination.
 */

#pragma once

class OTABoot {
public:
    void plan();
};
