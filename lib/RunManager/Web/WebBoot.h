/**
 * @file WebBoot.h
 * @brief Web interface one-time initialization
 * @version 251231E
 * @date 2025-12-31
 *
 * Handles web interface initialization during boot. Starts the async web
 * server and configures web policy. Part of the Boot→Plan→Policy→Run
 * pattern for web interface coordination.
 */

#pragma once

class WebBoot {
public:
    void plan();
};
