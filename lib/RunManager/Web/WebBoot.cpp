/**
 * @file WebBoot.cpp
 * @brief Web interface one-time initialization implementation
 * @version 260201A
 * @date 2026-02-01
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
