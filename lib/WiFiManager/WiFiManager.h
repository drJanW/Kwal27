/**
 * @file WiFiManager.h
 * @brief WiFi connection management with AP fallback
 * @version 251231E
 * @date 2025-12-31
 *
 * Header file for WiFi connection management.
 * Provides functions to initiate non-blocking WiFi connection
 * and check connection status. The module handles automatic
 * reconnection with growing interval and supports fallback
 * to Access Point preset when station connection fails.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>


void bootWiFiConnect();     // Begin non-blocking connection

#endif
