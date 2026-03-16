/**
 * @file SdPathUtils.h
 * @brief SD card path helper functions interface
 * @version 260316K
 * @date 2026-03-16
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

} // namespace SdPathUtils
