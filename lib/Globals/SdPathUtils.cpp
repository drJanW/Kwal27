/**
 * @file SdPathUtils.cpp
 * @brief SD card path helper functions implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * This file implements helper functions for SD card path manipulation.
 * It provides path sanitization (adding leading slash, removing trailing
 * slashes, blocking directory traversal), parent path extraction, filename
 * extraction, and safe file/directory deletion. These utilities ensure
 * consistent and secure path handling throughout the SD card operations.
 */

#include "SdPathUtils.h"

#include <SD.h>
#include <cstring>

#include "SDManager.h"

namespace SdPathUtils {

String sanitizeSdPath(const String &raw) {
    if (raw.length() == 0) {
        return String("/");
    }

    String path = raw;
    path.trim();
    if (path.length() == 0) {
        return String("/");
    }

    if (path[0] != '/') {
        path = "/" + path;
    }

    while (path.length() > 1 && path.endsWith("/")) {
        path.remove(path.length() - 1);
    }

    if (path.indexOf("..") >= 0) {
        return String();
    }

    if (path.length() >= SDPATHLENGTH) {
        return String();
    }

    return path;
}

String parentPath(const String &path) {
    if (path.length() <= 1) {
        return String("/");
    }
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash <= 0) {
        return String("/");
    }
    return path.substring(0, lastSlash);
}

String extractBaseName(const char *fullPath) {
    if (!fullPath) {
        return String();
    }
    const char *slash = strrchr(fullPath, '/');
    if (slash && slash[1] != '\0') {
        return String(slash + 1);
    }
    return String(fullPath);
}

bool removeSdPath(const String &targetPath, String &errorMessage) {
    File node = SD.open(targetPath.c_str(), FILE_READ);
    if (!node) {
        errorMessage = F("Path not found");
        return false;
    }
    const bool isDir = node.isDirectory();
    node.close();

    if (!isDir) {
        if (!SD.remove(targetPath.c_str())) {
            errorMessage = F("Delete failed");
            return false;
        }
        return true;
    }

    File dir = SD.open(targetPath.c_str(), FILE_READ);
    if (!dir) {
        errorMessage = F("Open directory failed");
        return false;
    }
    for (File child = dir.openNextFile(); child; child = dir.openNextFile()) {
        String childPath = targetPath;
        if (!childPath.endsWith("/")) {
            childPath += '/';
        }
        childPath += extractBaseName(child.name());
        child.close();
        if (!removeSdPath(childPath, errorMessage)) {
            dir.close();
            return false;
        }
    }
    dir.close();
    if (!SD.rmdir(targetPath.c_str())) {
        errorMessage = F("Remove directory failed");
        return false;
    }
    return true;
}

String sanitizeSdFilename(const String &raw) {
    if (raw.length() == 0) {
        return String();
    }
    String trimmed = raw;
    trimmed.trim();
    if (trimmed.length() == 0) {
        return String();
    }
    if (trimmed.indexOf('/') >= 0 || trimmed.indexOf('\\') >= 0) {
        return String();
    }
    if (trimmed.indexOf("..") >= 0) {
        return String();
    }
    return trimmed;
}

String buildUploadTarget(const String &directory, const String &filename) {
    String dir = sanitizeSdPath(directory);
    String name = sanitizeSdFilename(filename);
    if (dir.length() == 0 || name.length() == 0) {
        return String();
    }
    if (dir == "/") {
        return String("/") + name;
    }
    if (dir.endsWith("/")) {
        return dir + name;
    }
    return dir + "/" + name;
}

} // namespace SdPathUtils
