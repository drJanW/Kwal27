/**
 * @file TtsTodoQueue.cpp
 * @brief Self-consuming TTS render-queue from SD card
 * @version 260605B
 * @date 2026-06-05
 *
 * Implementation of plan() / cb_ttsTodoBoot / cb_ttsTodoNext.
 * See TtsTodoQueue.h and docs/pseudo_ttsqueue.md.
 */
#include <Arduino.h>
#include <SD.h>

#include "TtsTodoQueue.h"
#include "Globals.h"
#include "TimerManager.h"
#include "PlaySentence.h"
#include "SD/SDBoot.h"

namespace {

constexpr const char* CSV_PATH = "/tts_todo.csv";
constexpr const char* TMP_PATH = "/tts_todo.tmp";
constexpr uint32_t BOOT_DELAY_MS  = SECONDS(30);
constexpr uint32_t TICK_MS        = SECONDS(10);  // infinite-tick cadence
constexpr uint16_t MAX_LINE_LEN   = 240;     // Generous buffer for parser
constexpr uint16_t MAX_TEXT_LEN   = 160;     // VoiceRSS practical limit

struct ParsedLine {
    int8_t  voice;
    int8_t  tempo;
    uint8_t dir;
    uint8_t file;
    char    text[MAX_TEXT_LEN + 1];
};

// Trim leading/trailing whitespace in-place
void trim(char* s) {
    if (!s) return;
    char* p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\r' || s[n - 1] == '\n')) {
        s[--n] = '\0';
    }
}

// Skip blank lines and comments. Returns true if line is significant.
bool isActiveLine(const char* s) {
    if (!s || !*s) return false;
    if (s[0] == '#') return false;
    return true;
}

// Parse a single CSV line. Returns true if valid, false on any error.
// On error, errMsg gets a short reason (caller logs it with the raw line).
bool parseLine(const char* line, ParsedLine& out, const char*& errMsg) {
    // Find 5 semicolons; everything after the 5th is the text field.
    const char* p = line;
    const char* fieldStart[6];
    fieldStart[0] = p;
    uint8_t fieldIdx = 1;
    while (*p && fieldIdx < 6) {
        if (*p == ';') {
            fieldStart[fieldIdx++] = p + 1;
        }
        p++;
    }
    if (fieldIdx < 6) { errMsg = "fields<6"; return false; }

    // Copy fields 0..4 into local buffers and trim
    char buf[5][16];
    for (uint8_t i = 0; i < 5; i++) {
        size_t len = (fieldStart[i + 1] - 1) - fieldStart[i];
        if (len >= sizeof(buf[i])) { errMsg = "field too long"; return false; }
        memcpy(buf[i], fieldStart[i], len);
        buf[i][len] = '\0';
        trim(buf[i]);
    }

    // Field 0: lang — only NL accepted
    if (strcasecmp(buf[0], "NL") != 0) { errMsg = "lang!=NL"; return false; }

    // Field 1: voice — -1, 0, 1, 2
    int v = atoi(buf[1]);
    if (v < -1 || v > 2) { errMsg = "voice out of range"; return false; }
    out.voice = static_cast<int8_t>(v);

    // Field 2: tempo — -3..+3 or 99
    int t = atoi(buf[2]);
    if (!((t >= -3 && t <= 3) || t == 99)) { errMsg = "tempo out of range"; return false; }
    out.tempo = static_cast<int8_t>(t);

    // Field 3: dir — 1..200
    int d = atoi(buf[3]);
    if (d < 1 || d > 200) { errMsg = "dir out of range"; return false; }
    out.dir = static_cast<uint8_t>(d);

    // Field 4: file — 1..99
    int f = atoi(buf[4]);
    if (f < 1 || f > 99) { errMsg = "file out of range"; return false; }
    out.file = static_cast<uint8_t>(f);

    // Field 5: text — everything from fieldStart[5] to end of line
    size_t textLen = strlen(fieldStart[5]);
    if (textLen == 0) { errMsg = "empty text"; return false; }
    if (textLen > MAX_TEXT_LEN) { errMsg = "text too long"; return false; }
    strcpy(out.text, fieldStart[5]);
    trim(out.text);
    if (out.text[0] == '\0') { errMsg = "empty text"; return false; }

    return true;
}

// Read the first active line from CSV. Returns true if found.
// rawLine receives the original (untrimmed) bytes for logging.
bool readFirstActiveLine(char* rawLine, size_t rawLineSize) {
    File f = SD.open(CSV_PATH, FILE_READ);
    if (!f) return false;
    while (f.available()) {
        size_t n = f.readBytesUntil('\n', rawLine, rawLineSize - 1);
        rawLine[n] = '\0';
        // Skip BOM if present at start
        if (n >= 3 && (uint8_t)rawLine[0] == 0xEF &&
            (uint8_t)rawLine[1] == 0xBB && (uint8_t)rawLine[2] == 0xBF) {
            memmove(rawLine, rawLine + 3, n - 2);
        }
        char trimmed[MAX_LINE_LEN];
        strncpy(trimmed, rawLine, sizeof(trimmed) - 1);
        trimmed[sizeof(trimmed) - 1] = '\0';
        trim(trimmed);
        if (isActiveLine(trimmed)) {
            // Re-copy the trimmed version for parsing
            strcpy(rawLine, trimmed);
            f.close();
            return true;
        }
    }
    f.close();
    return false;
}

// Rewrite the CSV, commenting out the FIRST active line (prepend '# ').
// All other lines preserved verbatim. Atomic via .tmp.
bool commentOutFirstActiveLine() {
    File in = SD.open(CSV_PATH, FILE_READ);
    if (!in) return false;

    SD.remove(TMP_PATH);  // Ensure clean target
    File out = SD.open(TMP_PATH, FILE_WRITE);
    if (!out) { in.close(); return false; }

    bool commented = false;
    char line[MAX_LINE_LEN];
    while (in.available()) {
        size_t n = in.readBytesUntil('\n', line, sizeof(line) - 1);
        line[n] = '\0';

        if (!commented) {
            // Check if this is the line to comment out
            char trimmed[MAX_LINE_LEN];
            strncpy(trimmed, line, sizeof(trimmed) - 1);
            trimmed[sizeof(trimmed) - 1] = '\0';
            // Skip BOM for the check
            char* checkStr = trimmed;
            if ((uint8_t)checkStr[0] == 0xEF && (uint8_t)checkStr[1] == 0xBB &&
                (uint8_t)checkStr[2] == 0xBF) {
                checkStr += 3;
            }
            trim(checkStr);
            if (isActiveLine(checkStr)) {
                commented = true;
                out.write(reinterpret_cast<const uint8_t*>("# "), 2);
                out.write(reinterpret_cast<const uint8_t*>(line), n);
                out.write('\n');
                continue;
            }
        }
        out.write(reinterpret_cast<const uint8_t*>(line), n);
        out.write('\n');
    }
    in.close();
    out.close();

    if (!commented) {
        SD.remove(TMP_PATH);
        return false;
    }

    // Atomic swap
    SD.remove(CSV_PATH);
    if (!SD.rename(TMP_PATH, CSV_PATH)) {
        PF("[TTS-Q] rename %s -> %s FAILED\n", TMP_PATH, CSV_PATH);
        return false;
    }
    return true;
}

void cb_ttsTodoNext();  // forward

static uint8_t pendingDirSync_ = 0;  // Last dir written; sync on queue done

void cb_ttsTodoNext() {
    char rawLine[MAX_LINE_LEN];
    if (!readFirstActiveLine(rawLine, sizeof(rawLine))) {
        PL("[TTS-Q] queue done");
        timers.cancel(cb_ttsTodoNext);
        if (pendingDirSync_ != 0) {
            // Re-index the dir we just wrote to so MP3 grid sees new files
            SDBoot::requestSyncDir(pendingDirSync_);
            pendingDirSync_ = 0;
        }
        return;
    }

    ParsedLine pl;
    const char* errMsg = nullptr;
    if (!parseLine(rawLine, pl, errMsg)) {
        PF("[TTS-Q] parse err: %s | line: %s\n",
           errMsg ? errMsg : "?", rawLine);
        // Hard stop — line stays, user must fix and reboot.
        timers.cancel(cb_ttsTodoNext);
        return;
    }

    PF("[TTS-Q] render %u/%03u: %s\n", pl.dir, pl.file, pl.text);

    uint32_t durationMs = PlaySentence::downloadTtsToCache(
        pl.text, pl.voice, pl.tempo, pl.file, pl.dir);

    if (durationMs == 0) {
        PL("[TTS-Q] render failed (network/SD), retrying");
        return;  // infinite timer retries automatically
    }

    if (!commentOutFirstActiveLine()) {
        PL("[TTS-Q] CSV rewrite failed — will retry same line next boot");
        timers.cancel(cb_ttsTodoNext);
        return;
    }

    PF("[TTS-Q] OK %u/%03u (%ums)\n", pl.dir, pl.file, durationMs);
    pendingDirSync_ = pl.dir;
    // Next tick scheduled automatically by infinite timer (repeat=0).
}

void cb_ttsTodoBoot() {
    // Crash recovery: if .tmp exists but .csv missing → swap is half-done
    bool hasCsv = SD.exists(CSV_PATH);
    bool hasTmp = SD.exists(TMP_PATH);
    if (hasTmp && !hasCsv) {
        if (SD.rename(TMP_PATH, CSV_PATH)) {
            PL("[TTS-Q] recovered .tmp -> .csv");
            hasCsv = true;
        }
    } else if (hasTmp && hasCsv) {
        SD.remove(TMP_PATH);  // Stale tmp from previous run, discard
    }

    if (!hasCsv) {
        return;  // Nothing to do
    }

    // Quick check: any active lines at all?
    char probe[MAX_LINE_LEN];
    if (!readFirstActiveLine(probe, sizeof(probe))) {
        return;  // CSV exists but contains only comments / blank lines
    }

    PL("[TTS-Q] queue starting");
    // Infinite tick (repeat=0) — callback self-cancels when queue done
    // or on a hard error. This avoids the self-reschedule trap where
    // create() inside a one-shot fails because the slot is still active.
    timers.create(TICK_MS, 0, cb_ttsTodoNext);
}

}  // anonymous namespace

namespace TtsTodoQueue {

void plan() {
    // Deferred boot scan — give WiFi + SD time to stabilize after verdict
    timers.create(BOOT_DELAY_MS, 1, cb_ttsTodoBoot);
}

void startNow() {
    timers.cancel(cb_ttsTodoBoot);  // annuleer eventuele lopende boot-delay
    cb_ttsTodoBoot();               // direct uitvoeren
}

}  // namespace TtsTodoQueue
