/**
 * @file CsvUtils.h
 * @brief CSV parsing utilities interface
 * @version 260202A
 $12026-02-05
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
