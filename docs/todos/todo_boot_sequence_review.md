# Boot Sequence Review

## Problem
The boot sequence uses manual continuation-passing with hardcoded functions and boolean guards:
- `begin()` → `resumeAfterSDBoot()` → `resumeAfterWiFiBoot()`
- Each step is a static function with a `bool xxxCompleted` flag
- No queue, no state machine, no sequencer
- Adding a new async step (like PNF calibration) requires yet another `resumeAfterXxx()` + flag

## Current pain points
1. PNF calibration needs to pause between `lightRun.plan()` and `audioBoot.plan()` — no clean way without another continuation function
2. Each `resumeAfter*` duplicates the guard pattern
3. Boot order is implicit in code, not declarative
4. Hard to reason about what runs when — requires reading 3 functions across 3 files

## Possible improvements
- Boot step queue: array of `{name, function, asyncCallback}` executed sequentially
- State machine with named phases
- Single `advanceBoot()` that pops next step from queue
- Each async step calls `advanceBoot()` when done (replaces all `resumeAfter*`)

## Scope
Full RunManager boot refactor. Not urgent — current system works, just hard to extend.
