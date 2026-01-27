/**
 * @file SdHelpers.h
 * @brief SD card file serving utilities
 * @version 251231E
 * @date 2025-12-31
 *
 * Header providing SD card path utilities for the web interface.
 * This is a legacy shim that re-exports functions from SdPathUtils.h.
 * Functions include path sanitization, parent path extraction, basename
 * extraction, and upload target building for safe SD card file operations.
 */

#pragma once

#include "SdPathUtils.h"

// Legacy shim: prefer including SdPathUtils.h directly from lib/Common.
namespace WebSdHelpers {
using SdPathUtils::sanitizeSdPath;
using SdPathUtils::parentPath;
using SdPathUtils::extractBaseName;
using SdPathUtils::removeSdPath;
using SdPathUtils::sanitizeSdFilename;
using SdPathUtils::buildUploadTarget;
} // namespace WebSdHelpers
