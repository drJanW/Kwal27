/**
 * @file CsvUtils.h
 * @brief CSV parsing utilities interface
 * @version 251231E
 * @date 2025-12-31
 *
 * This header declares CSV parsing utilities for reading and processing
 * semicolon-delimited CSV files from the SD card. Provides a namespace-based
 * API for line reading, column splitting, and BOM handling. These utilities
 * are used by calendar, color, pattern, and shift managers to load their
 * configuration data from CSV files.
 */

#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

namespace csv {

// Reads the next line from the file (stripping CR/LF). Returns false on EOF.
bool readLine(File& file, String& out);

// Splits a semicolon-separated line into trimmed columns.
void splitColumns(const String& line, std::vector<String>& columns, char delimiter = ';');

// Removes a UTF-8 BOM prefix from the provided string if present.
void stripBom(String& text);

} // namespace csv
