/**
 * @file AlertBoot.cpp
 * @brief Alert system one-time initialization implementation
 * @version 260131A
 $12026-02-05
 */
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "AlertBoot.h"
#include "AlertRun.h"
#include "Globals.h"

void AlertBoot::configure() {
    AlertRun::plan();
    PL("[*Boot] AlertRun configured");
}