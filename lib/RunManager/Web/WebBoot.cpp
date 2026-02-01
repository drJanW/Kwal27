/**
 * @file WebBoot.cpp
 * @brief Web interface one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements web boot sequence: initializes WebInterfaceController, enables
 * web interface, and configures WebPolicy settings.
 */

#include <Arduino.h>
#include "WebBoot.h"
#include "Globals.h"
#include "WebInterfaceController.h"
#include "WebPolicy.h"
#include "LightController.h"

void WebBoot::plan() {
    beginWebInterface();
    WebPolicy::configure();
}
