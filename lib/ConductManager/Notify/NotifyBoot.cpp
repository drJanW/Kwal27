/**
 * @file NotifyBoot.cpp
 * @brief Notification system one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements notification boot sequence: configures NotifyConduct to be
 * ready for receiving hardware status reports during system startup.
 */

#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "NotifyBoot.h"
#include "NotifyConduct.h"
#include "Globals.h"

void NotifyBoot::configure() {
    NotifyConduct::plan();
    PL("[*Boot] NotifyConduct configured");
}
