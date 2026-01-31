/**
 * @file TodayContext.cpp
 * @brief Today's context state management implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements the today context management system combining calendar data,
 * theme boxes, colors, and patterns into a unified daily context.
 * Handles initialization from SD card, loading current day's context,
 * and providing access to theme boxes, patterns, and colors for the
 * current date. Central to date-aware behavior throughout the system.
 */

#include "TodayContext.h"

#include "Globals.h"
#include "PRTClock.h"
#include "SdPathUtils.h"

// Forward declarations for LightRun request methods (Rule 1.5)
class LightRun {
public:
    static bool describePatternById(uint8_t id, LightPattern& out);
    static bool describeActivePattern(LightPattern& out);
    static bool describeColorById(uint8_t id, LightColor& out);
    static bool describeActiveColor(LightColor& out);
};

namespace {

struct TodayContextLogLimiter {
    uint32_t lastNoCalendar{0};
    uint32_t lastThemeFallback{0};
    uint32_t lastThemeUnavailable{0};
    uint32_t lastPatternFallback{0};
    uint32_t lastPatternUnavailable{0};
    uint32_t lastColorFallback{0};
    uint32_t lastColorUnavailable{0};

    static uint32_t makeKey(uint16_t year, uint8_t month, uint8_t day) {
        return (static_cast<uint32_t>(year) << 16) |
               (static_cast<uint32_t>(month) << 8) |
               static_cast<uint32_t>(day);
    }

    static bool shouldLog(uint32_t &slot, uint16_t year, uint8_t month, uint8_t day) {
        const uint32_t key = makeKey(year, month, day);
        if (slot == key) {
            return false;
        }
        slot = key;
        return true;
    }

    bool logNoCalendar(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastNoCalendar, year, month, day);
    }

    bool logThemeFallback(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastThemeFallback, year, month, day);
    }

    bool logThemeUnavailable(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastThemeUnavailable, year, month, day);
    }

    bool logPatternFallback(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastPatternFallback, year, month, day);
    }

    bool logPatternUnavailable(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastPatternUnavailable, year, month, day);
    }

    bool logColorFallback(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastColorFallback, year, month, day);
    }

    bool logColorUnavailable(uint16_t year, uint8_t month, uint8_t day) {
        return shouldLog(lastColorUnavailable, year, month, day);
    }
};

TodayContextLogLimiter g_logLimiter;

enum class LoaderLogState : uint8_t {
    Unknown,
    Ready,
    NotReady
};

struct LoaderInitLogState {
    bool invalidRoot = false;
    bool calendarInitFailed = false;
    bool themeBoxInitFailed = false;
};

LoaderLogState g_loaderLogState = LoaderLogState::Unknown;
LoaderInitLogState g_loaderInitLogs;

class TodayContextLoader {
public:
    bool init(fs::FS& sd, const char* rootPath);
    bool ready() const { return ready_; }
    bool loadToday(TodayContext& ctx);
    const ThemeBox* findThemeBox(uint8_t id) const { return themeBoxes_.find(id); }
    const ThemeBox* getDefaultThemeBox() const { return themeBoxes_.active(); }

private:
    bool resolveDate(uint16_t& year, uint8_t& month, uint8_t& day) const;

    Context::CalendarManager calendar_;
    ThemeBoxManager themeBoxes_;
    String root_{"/"};
    bool ready_{false};
};

TodayContextLoader g_loader;

bool TodayContextLoader::init(fs::FS& sd, const char* rootPath) {
    ready_ = false;
    const String desiredRoot = (rootPath && *rootPath) ? String(rootPath) : String("/");
    const String sanitized = SdPathUtils::sanitizeSdPath(desiredRoot);
    if (sanitized.isEmpty()) {
        if (!g_loaderInitLogs.invalidRoot) {
            PF("[TodayContext] Invalid root '%s', falling back to '/'\n", desiredRoot.c_str());
            g_loaderInitLogs.invalidRoot = true;
        }
        root_ = "/";
    } else {
        g_loaderInitLogs.invalidRoot = false;
        root_ = sanitized;
    }

    const char* rootCStr = root_.c_str();

    if (!calendar_.begin(sd, rootCStr)) {
        if (!g_loaderInitLogs.calendarInitFailed) {
            PF("[TodayContext] CalendarManager init failed\n");
            g_loaderInitLogs.calendarInitFailed = true;
        }
        return false;
    }
    g_loaderInitLogs.calendarInitFailed = false;
    if (!themeBoxes_.begin(sd, rootCStr)) {
        if (!g_loaderInitLogs.themeBoxInitFailed) {
            PF("[TodayContext] ThemeBoxManager init failed\n");
            g_loaderInitLogs.themeBoxInitFailed = true;
        }
        return false;
    }
    g_loaderInitLogs.themeBoxInitFailed = false;

    ready_ = true;
    if (g_loaderLogState != LoaderLogState::Ready) {
        PF("[TodayContext] Loader initialised\n");
        g_loaderLogState = LoaderLogState::Ready;
    }
    return true;
}

bool TodayContextLoader::resolveDate(uint16_t& year, uint8_t& month, uint8_t& day) const {
    const uint16_t rawYear = prtClock.getYear();
    month = prtClock.getMonth();
    day = prtClock.getDay();
    if (rawYear == 0 || month == 0 || day == 0) {
        return false;
    }

    if (rawYear >= 1900) {
        year = rawYear;
    } else {
        year = static_cast<uint16_t>(2000 + rawYear);
    }
    return true;
}

bool TodayContextLoader::loadToday(TodayContext& ctx) {
    ctx = TodayContext{};
    if (!ready_) {
        if (g_loaderLogState != LoaderLogState::NotReady) {
            PF("[TodayContext] Loader not ready\n");
            g_loaderLogState = LoaderLogState::NotReady;
        }
        return false;
    }

    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    if (!resolveDate(year, month, day)) {
        return false;
    }

    CalendarEntry entry;
    const bool hasCalendarEntry = calendar_.findEntry(year, month, day, entry);
    if (!hasCalendarEntry) {
        entry.valid = false;
        entry.year = year;
        entry.month = month;
        entry.day = day;
        if (g_logLimiter.logNoCalendar(year, month, day)) {
            PF("[TodayContext] No calendar entry for %04u-%02u-%02u, using defaults\n",
               static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        }
    }

    const ThemeBox* theme = nullptr;
    if (hasCalendarEntry && entry.themeBoxId != 0) {
        theme = themeBoxes_.find(entry.themeBoxId);
    }
    if (!theme) {
        const ThemeBox* fallbackTheme = themeBoxes_.active();
        if (fallbackTheme) {
            if (entry.themeBoxId != 0 && g_logLimiter.logThemeFallback(year, month, day)) {
                PF("[TodayContext] Theme box %u missing, falling back to %u for %04u-%02u-%02u\n",
                   static_cast<unsigned>(entry.themeBoxId),
                   static_cast<unsigned>(fallbackTheme->id),
                   static_cast<unsigned>(year),
                   static_cast<unsigned>(month),
                   static_cast<unsigned>(day));
            }
            theme = fallbackTheme;
        }
    }
    if (!theme) {
        if (g_logLimiter.logThemeUnavailable(year, month, day)) {
            PF("[TodayContext] No theme boxes available for %04u-%02u-%02u\n",
               static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        }
        return false;
    }

    // Lookup pattern via LightRun request (Rule 1.5)
    LightPattern pattern;
    bool hasPattern = false;
    if (hasCalendarEntry && entry.patternId != 0) {
        hasPattern = LightRun::describePatternById(entry.patternId, pattern);
    }
    if (!hasPattern) {
        LightPattern fallbackPattern;
        if (LightRun::describeActivePattern(fallbackPattern)) {
            if (entry.patternId != 0 && g_logLimiter.logPatternFallback(year, month, day)) {
                PF("[TodayContext] Pattern %u missing, falling back to %u for %04u-%02u-%02u\n",
                   static_cast<unsigned>(entry.patternId),
                   static_cast<unsigned>(fallbackPattern.id),
                   static_cast<unsigned>(year),
                   static_cast<unsigned>(month),
                   static_cast<unsigned>(day));
            }
            pattern = fallbackPattern;
            hasPattern = true;
        }
    }
    if (!hasPattern) {
        if (g_logLimiter.logPatternUnavailable(year, month, day)) {
            PF("[TodayContext] No light patterns available for %04u-%02u-%02u\n",
               static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        }
        return false;
    }

    // Lookup color via LightRun request (Rule 1.5)
    LightColor color;
    bool hasColor = false;
    if (hasCalendarEntry && entry.colorId != 0) {
        hasColor = LightRun::describeColorById(entry.colorId, color);
    }
    if (!hasColor) {
        LightColor fallbackColor;
        if (LightRun::describeActiveColor(fallbackColor)) {
            if (entry.colorId != 0 && g_logLimiter.logColorFallback(year, month, day)) {
                PF("[TodayContext] Color %u missing, falling back to %u for %04u-%02u-%02u\n",
                   static_cast<unsigned>(entry.colorId),
                   static_cast<unsigned>(fallbackColor.id),
                   static_cast<unsigned>(year),
                   static_cast<unsigned>(month),
                   static_cast<unsigned>(day));
            }
            color = fallbackColor;
            hasColor = true;
        }
    }
    if (!hasColor) {
        if (g_logLimiter.logColorUnavailable(year, month, day)) {
            PF("[TodayContext] No light colors available for %04u-%02u-%02u\n",
               static_cast<unsigned>(year), static_cast<unsigned>(month), static_cast<unsigned>(day));
        }
        return false;
    }

    ctx.valid = true;
    ctx.entry = entry;
    ctx.theme = *theme;
    ctx.pattern = pattern;
    ctx.colors = color;
    return true;
}

} // namespace

bool InitTodayContext(fs::FS& sd, const char* rootPath) {
    return g_loader.init(sd, rootPath);
}

bool TodayContextReady() {
    return g_loader.ready();
}

bool LoadTodayContext(TodayContext& ctx) {
    return g_loader.loadToday(ctx);
}

const ThemeBox* FindThemeBox(uint8_t id) {
    if (!g_loader.ready()) {
        return nullptr;
    }
    return g_loader.findThemeBox(id);
}

const ThemeBox* GetDefaultThemeBox() {
    if (!g_loader.ready()) {
        return nullptr;
    }
    return g_loader.getDefaultThemeBox();
}
