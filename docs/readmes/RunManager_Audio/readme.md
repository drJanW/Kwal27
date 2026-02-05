# Audio Run – Distance Playback Notes

> Version: 260205D | Updated: 2026-02-05

This file captures the current understanding of how distance driven PCM playback is coordinated with the rest of the audio stack. It is mostly here as a reminder for future maintenance sessions.

## Roles
- **AudioPolicy** decides if a distance reading should produce audio at all and exposes any ancillary knobs (volume shaping, etc.).
- **AudioRun** interprets policy results, keeps the most recent distance measurement, and schedules PCM playback attempts through the timer layer using the policy-defined interval (when policy declines we park the timer far in the future). The distance clip pointer is registered once during boot via `setDistanceClipPointer()` and read with `getDistanceClipPointer()` wherever needed.
- **AudioManager** exposes the live playback state (fragments, sentences, PCM clips) and executes stop/start requests.
- **TimerManager** owns the actual timers; `AudioRun` never runs its own scheduler, it only asks `TimerManager::restart` to re-arm the single callback.

## PCM vs fragment sequencing
1. When a new distance reading arrives, the sensor runner checks `AudioPolicy::distancePlaybackInterval` purely for eligibility. If policy declines we park the timer with a long interval; otherwise we call `AudioRun::startDistanceResponse` once on entry (and again after each playback completes).
2. `startDistanceResponse` requests a fragment stop when needed and arms the timer using `AudioPolicy::distancePlaybackInterval`; when policy refuses we set a one-megasecond parking interval. The callback will pick the distance up again through `SensorsPolicy` when it finally fires.

## Timer usage
- `AudioRun` calls `TimerManager::restart` directly whenever a distance response is started or refreshed. There is a single timer slot for the callback; if the restart fails we log it and abort the attempt.
- We do not keep side-band timer state; the timer system remains the single source of truth.

## Policy requirements
- `AudioPolicy::distancePlaybackInterval` provides both the eligibility decision and the interval. If the policy rejects the reading we park the timer at one million milliseconds and wait for a fresh distance to re-arm it.
- If a fragment is still winding down the first PCM request is refused; the timer-driven retries are the sanctioned way to wait until audio is free.
- After a fragment stop is issued the next distance playback always restarts from the top; there is no attempt to resume an old clip.

## Theme box coordination
- `AudioPolicy::setThemeBox` stores the active theme identifier plus up to sixteen fragment directories supplied by the calendar run. When the calendar clears the theme we call `AudioPolicy::clearThemeBox` to remove the allow-list.
- `AudioDirector` now operates purely on the SD index weights (`fileCount`/`totalScore`). If a theme allow-list yields zero weighted directories it logs `[AudioDirector] No weighted directories for active theme filter` and returns failure instead of falling back to the global pool.
- Callers must surface that failure (today the runner keeps the previous fragment playing) so we can spot stale theme data and fix the index rather than masking it.
- Calendar updates arrive on the hourly interval; the policy swap happens synchronously so the next fragment pick honours the new theme without needing an explicit audio reset.

## Weighted fragment picker
- `AudioDirector::selectRandomFragment` only considers directories and files whose weights are non-zero in `.root_dirs`/`.files_dir`. Any hole (zero score) results in an early return and an explicit log line.
- Ensure `SDController::rebuildIndex()` runs after adjusting scores; with fallbacks removed the director will refuse to play if the index is stale or empty.
- Expect log noise when the SD data is incomplete—that is intentional so broken scoring does not silently continue.

## Callback behaviour
- `cb_playPCM()` checks whether other audio is still busy. If fragments or sentences are active it re-arms itself with the busy retry window; otherwise it fetches the latest cached distance via `SensorsPolicy::currentDistance`, asks `AudioPolicy::updateDistancePlaybackVolume` for the current multiplier, and starts PCM playback with the resulting level.
- Actual playback is attempted via `PlayPCM::play`. A failure here is logged but otherwise the logic relies on the next distance event (or a fresh `setDistanceClipPointer()` call) to recover.

These notes should keep the interaction between policy, run, and the timers straight without re-learning the same rules next time.
