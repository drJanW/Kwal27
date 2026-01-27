/**
 * @file OTABoot.cpp
 * @brief OTA update one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements OTA boot sequence: configures OTAPolicy. OTA is not required
 * during boot sequence as updates are handled via OTAManager separately.
 */

#include "OTABoot.h"
#include "Globals.h"
#include "OTAPolicy.h"

void OTABoot::plan() {
    // Boot stub - OTA not required during boot sequence
    OTAPolicy::configure();
}
