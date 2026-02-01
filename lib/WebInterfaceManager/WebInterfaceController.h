/**
 * @file WebInterfaceController.h
 * @brief Async web server setup, routes index.html and API endpoints
 * @version 251231E
 * @date 2025-12-31
 *
 * Header for the main web interface controller. Provides public functions
 * to initialize the async web server and handle periodic updates.
 * The server exposes REST API endpoints for audio, lights, patterns,
 * colors, context, SD card operations, OTA updates, and health checks.
 */

#ifndef WEB_INTERFACE_CONTROLLER_H
#define WEB_INTERFACE_CONTROLLER_H

void beginWebInterface();
void updateWebInterface();

#endif
