/**
 * @file SDPolicy.cpp
 * @brief SD card file operation business logic implementation
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements SD file operation rules: delegates fragment selection to
 * AudioDirector, enforces audio-idle requirement for file deletion, and
 * provides status logging for diagnostics.
 */

#include "SDPolicy.h"
#include "SDVoting.h"       // weighted voting/selection helpers
#include "Globals.h"        // for PF/PL macros if needed
#include "AudioState.h"     // isAudioBusy, isSentencePlaying
#include "Audio/AudioDirector.h"
#include "Notify/NotifyState.h"

namespace SDPolicy {

bool getRandomFragment(AudioFragment& outFrag) {
    // Delegate to director for weighted selection (will expand with context)
    return AudioDirector::selectRandomFragment(outFrag);
}

bool deleteFile(uint8_t dirIndex, uint8_t fileIndex) {
    // Policy: only delete if audio is idle
    if (!isAudioBusy() && !isSentencePlaying()) {
        SDVoting::deleteIndexedFile(dirIndex, fileIndex);
        return true;
    }
    // Otherwise reject (RunManager can retry later)
    PF("[SDPolicy] Reject delete: audio busy\n");
    return false;
}

namespace {
bool stateInitialized = false;
bool lastReady = false;
bool lastBusy = false;
}

void showStatus(bool forceLog) {
    const bool ready = NotifyState::isSdOk();
    const bool busy = NotifyState::isSdBusy();

    if (!forceLog && stateInitialized && ready == lastReady && busy == lastBusy) {
        return;
    }

    stateInitialized = true;
    lastReady = ready;
    lastBusy = busy;

    PF("[SDPolicy] SD ready=%d busy=%d\n", ready, busy);
    // Could add more diagnostics here (e.g. number of indexed files)
}

}
