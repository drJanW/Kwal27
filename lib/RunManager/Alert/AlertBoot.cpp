/**
 * @file AlertBoot.cpp
 * @brief Alert system one-time initialization implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements alert boot sequence: configures AlertRun to be
 * ready for receiving hardware status reports during system startup.
 */

#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "AlertBoot.h"
#include "AlertRun.h"
#include "Globals.h"

void AlertBoot::configure() {
    AlertRun::plan();
    PL("[*Boot] AlertRun configured");
}