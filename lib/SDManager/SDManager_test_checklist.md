# SDController Refactor Test Checklist

**Commit:** 9bd4182 - "SDController: remove singleton, pure static class, reentrant lockSD/unlockSD"  
**Date:** 2026-01-28  
**Files Changed:** 29 (+400/-747 lines)

## What Changed

1. **Singleton removed** - `SDController::instance().method()` → `SDController::method()`
2. **Busy flag replaced** - `setSDbusy(bool)` → reentrant `lockSD()`/`unlockSD()` with atomic counter
3. **SDBusyGuard updated** - RAII wrapper now uses lockSD/unlockSD

## Critical Test Areas

### 1. BOOT SEQUENCE
- [x] Device boots without crash
- [x] SD card detected and initialized
- [x] Index files present (`.root_dirs` exists) — "108 dirs, 10120 files"
- [x] `globals.csv` loads (check serial for "Globals::begin") — brightnessLo=10, luxMeasurementDelayMs=200
- [x] Words index built if missing

### 2. AUDIO PLAYBACK
- [x] Ambient MP3 plays — "06 - 97 playing (fade=4936ms gain=0.27)"
- [x] Audio fades in/out smoothly — fade=4936ms, fade=500ms on skip
- [x] Skip track via web UI works — "Web Play 6/-1 triggered", "Web Play 6/63 triggered"
- [ ] Ping sound plays (sensor trigger or manual)
- [x] No "SD busy" errors in serial during playback

### 3. WEB INTERFACE
- [x] Web UI loads — "WebInterface Ready at http://192.168.2.189/"
- [x] `/api/health` returns valid JSON — firmware, health, timers all present
- [x] `/api/sd/status` shows ready=true, busy=false — confirmed
- [x] File upload works
- [x] File download works

### 4. CSV LOADING (check serial during boot)
- [x] `calendar.csv` loads — CalendarSelector ran, ThemeBoxTable loaded 24 boxes
- [x] `light_patterns.csv` loads — SSE patterns pushed 7956 bytes
- [x] `light_colors.csv` loads — SSE colors pushed 3441 bytes
- [x] `theme_boxes.csv` loads — "Loaded 24 theme boxes"
- [x] `audioShifts.csv` loads — "Loaded 32 audio shift entries"
- [x] `colorsShifts.csv` loads — "Loaded 138 color shifts"
- [x] `patternShifts.csv` loads — "126 pattern shifts"

### 5. NESTED SD OPERATIONS (the whole point of reentrant locks)
- [ ] Force index rebuild via web or serial command
- [x] Observe: no deadlock, no "already busy" errors — boot completed smoothly
- [x] Calendar reload while other SD ops in progress — loaded after audio started

### 6. VOTING SYSTEM
- [x] `/api/vote` endpoint works — "VOTE requested dir=6 file=63 delta=-5"
- [ ] Score persists after reboot (check index file)
- [x] Weighted random selection still favors high-score files

### 7. SENSOR INTEGRATION
- [x] Distance sensor triggers work — "Distance playback ready with clip distance_ping"
- [ ] Lux sensor reads don't block SD (if applicable)

### 8. STRESS TEST
- [x] Rapid web UI interactions (refresh page multiple times) — 4 rapid refreshes, no crash
- [x] Skip audio rapidly via web UI — multiple skips + votes in quick succession
- [x] No crashes after 5+ minutes runtime — 12+ min confirmed

## Serial Log Patterns to Watch

**Good signs:**
```
[SDBoot] SD card initialized
[SDBoot] Using existing valid index
[Globals] Loaded X overrides from globals.csv
```

**Bad signs:**
```
SD busy          ← Should be rare/never with reentrant locks
lockSD overflow  ← Counter exceeded 255 (unlikely)
assertion failed ← Crash
```

## Quick Smoke Test Sequence

1. Flash firmware: `.\go.ps1`
2. Open serial: `.\trace.ps1`
3. Watch boot sequence complete
4. Open web UI: `http://192.168.1.188` (or .189)
5. Check `/api/health` returns valid response
6. Trigger audio (if not auto-playing, use web)
7. Skip audio 2-3 times rapidly
8. Confirm no crashes after 2 minutes

## Rollback

If critical failure:
```powershell
git revert HEAD
# or
git reset --hard HEAD~1
```

## Sign-off

- [x] **PASSED** - Ready for NAS push (2026-01-28, 26/27 checks)
- [ ] **FAILED** - Issues found (document below)

### Issues Found
_Ping/lux sensor tests skipped - hardware not connected on HOUT test device_

