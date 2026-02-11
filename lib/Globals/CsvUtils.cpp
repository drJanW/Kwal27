/**
 * @file CsvUtils.cpp
 * @brief CSV parsing utilities implementation
 * @version 260202A
 $12026-02-05
 */
#include "CsvUtils.h"

namespace csv {

namespace {

inline void trimTrailingNewline(String& line) {
    line.trim();
}

} // namespace

bool readLine(File& file, String& out) {
    if (!file || !file.available()) {
        return false;
    }

    out = file.readStringUntil('\n');
    trimTrailingNewline(out);
    stripBom(out);
    return true;
}

void stripBom(String& text) {
    if (text.length() >= 3) {
        const uint8_t b0 = static_cast<uint8_t>(text.charAt(0));
        const uint8_t b1 = static_cast<uint8_t>(text.charAt(1));
        const uint8_t b2 = static_cast<uint8_t>(text.charAt(2));
        if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) {
            text.remove(0, 3);
        }
    }
}

void splitColumns(const String& line, std::vector<String>& columns, char delimiter) {
    columns.clear();
    int start = 0;
    const int len = line.length();
    while (start <= len) {
        int idx = line.indexOf(delimiter, start);
        String token;
        if (idx < 0) {
            token = line.substring(start);
            columns.push_back(token);
            break;
        }
        token = line.substring(start, idx);
        columns.push_back(token);
        start = idx + 1;
        if (start == len) {
            columns.push_back(String());
            break;
        }
    }

    for (auto& column : columns) {
        column.trim();
    }
}

} // namespace csv
