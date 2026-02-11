/**
 * @file OTABoot.cpp
 * @brief OTA update one-time initialization implementation
 * @version 260201A
 $12026-02-05
 */
#include "OTABoot.h"
#include "Globals.h"
#include "OTAPolicy.h"

void OTABoot::plan() {
    // Boot stub - OTA not required during boot sequence
    OTAPolicy::configure();
}
