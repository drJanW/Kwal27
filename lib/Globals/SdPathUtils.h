/**
 * @file SdPathUtils.h
 * @brief SD card path helper functions interface
 * @version 251231E
 * @date 2025-12-31
 *
 * This header declares helper functions for SD card path manipulation.
 * Provides utilities for path sanitization, parent path extraction,
 * filename extraction, and safe file operations. These functions ensure
 * consistent path formatting and security against directory traversal
 * attacks throughout the SD card subsystem.
 */

#pragma once

#include <Arduino.h>

namespace SdPathUtils {

String sanitizeSdPath(const String &raw);
String parentPath(const String &path);
String extractBaseName(const char *fullPath);
bool removeSdPath(const String &targetPath, String &errorMessage);
String sanitizeSdFilename(const String &raw);
String buildUploadTarget(const String &directory, const String &filename);
String chooseCsvPath(const char *filename);

} // namespace SdPathUtils
