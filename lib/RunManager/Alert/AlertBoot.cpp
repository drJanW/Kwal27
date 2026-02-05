/**
 * @file AlertBoot.cpp
 * @brief Alert system one-time initialization implementation
 * @version 260131A
 * @date 2026-01-31
 */
#define LOCAL_LOG_LEVEL LOG_LEVEL_INFO
#include "AlertBoot.h"
#include "AlertRun.h"
#include "Globals.h"

void AlertBoot::configure() {
    AlertRun::plan();
    PL("[*Boot] AlertRun configured");
}