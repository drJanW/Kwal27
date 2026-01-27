/**
 * @file WebBoot.cpp
 * @brief Web interface one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements web boot sequence: initializes WebInterfaceManager, enables
 * web interface, and configures WebPolicy settings.
 */

#include "WebBoot.h"
#include "Globals.h"
#include "WebInterfaceManager.h"
#include "WebPolicy.h"
#include "LightManager.h"

void WebBoot::plan() {
    beginWebInterface();
    WebPolicy::configure();
}
