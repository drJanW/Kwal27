/**
 * @file SDVoting.cpp
 * @brief Audio fragment voting system implementation with score tracking per file
 * @version 260202A
 * @date 2026-02-02
 */
#include <Arduino.h>
#include "SDVoting.h"
#include "Globals.h"
#include "MathUtils.h"
#include "AudioState.h"
#include "ContextController.h"
#include "SDController.h"
#include "WebGuiStatus.h"
#include "Alert/AlertState.h"

#include <ESPAsyncWebServer.h>
#ifdef ARDUINO_ARCH_ESP32
  #include <AsyncTCP.h>
#endif

#ifndef WEBIF_LOG_LEVEL
#define WEBIF_LOG_LEVEL 0
#endif

#if WEBIF_LOG_LEVEL && !defined(WEBIF_LOG)
#define WEBIF_LOG(...) PF(__VA_ARGS__)
#elif !defined(WEBIF_LOG)
#define WEBIF_LOG(...) do {} while (0)
#endif

namespace {

int16_t totalVote = 0;   // Accumulated vote delta, saved to SD when free

bool readCurrentScore(uint8_t dir, uint8_t file, uint8_t& scoreOut) {
  if (AlertState::isSdBusy()) {
    PF("[SDVoting] Busy while reading score %03u/%03u\n", dir, file);
    scoreOut = 0;
    return false;
  }
  SDController::lockSD();

  FileEntry fe;
  if (!SDController::readFileEntry(dir, file, &fe)) {
    scoreOut = 0;
    SDController::unlockSD();
    return false;
  }
  scoreOut = fe.score;
  SDController::unlockSD();
  return true;
}

} // namespace

uint8_t SDVoting::getRandomFile(uint8_t dir_num) {
  if (AlertState::isSdBusy()) {
    PF("[SDVoting] Busy while selecting file from dir %03u\n", dir_num);
    return 0;
  }
  SDController::lockSD();

  DirEntry dir;
  if (!SDController::readDirEntry(dir_num, &dir) || dir.fileCount == 0) {
    SDController::unlockSD();
    return 0;
  }

  uint8_t  fileNums[SD_MAX_FILES_PER_SUBDIR];
  uint8_t  fileScores[SD_MAX_FILES_PER_SUBDIR];
  uint8_t  count = 0;
  uint16_t total = 0;

  for (uint8_t i = 1; i <= SD_MAX_FILES_PER_SUBDIR; i++) {
    FileEntry fe;
  if (SDController::readFileEntry(dir_num, i, &fe) && fe.score > 0) {
      fileNums[count]   = i;
      fileScores[count] = fe.score;
      total += fe.score;
      if (++count >= dir.fileCount) break;
    }
  }
  if (count == 0) {
    SDController::unlockSD();
    return 0;
  }

  uint16_t pick = random(1, total + 1), acc = 0;
  for (uint8_t i = 0; i < count; i++) {
    acc += fileScores[i];
    if (pick <= acc) {
      SDController::unlockSD();
      return fileNums[i];
    }
  }
  SDController::unlockSD();
  return fileNums[0];
}

uint8_t SDVoting::applyVote(uint8_t dir_num, uint8_t file_num, int8_t delta) {
  SDController::lockSD();

  FileEntry fe; DirEntry dir;
  if (!SDController::readFileEntry(dir_num, file_num, &fe)) {
    SDController::unlockSD();
    return 0;
  }
  if (!SDController::readDirEntry(dir_num, &dir)) {
    SDController::unlockSD();
    return 0;
  }
  if (fe.score == 0) {
    SDController::unlockSD();
    return 0;
  }

  int16_t ns = static_cast<int16_t>(fe.score) + static_cast<int16_t>(delta);
  ns = MathUtils::clamp(ns, static_cast<int16_t>(1), static_cast<int16_t>(200));

  dir.totalScore += (ns - fe.score);
  fe.score = (uint8_t)ns;

  SDController::writeFileEntry(dir_num, file_num, &fe);
  SDController::writeDirEntry(dir_num, &dir);
  
  SDController::unlockSD();
  return fe.score;
}

void SDVoting::banFile(uint8_t dir_num, uint8_t file_num) {
  if (AlertState::isSdBusy()) {
    PF("[SDVoting] Busy while banning %03u/%03u\n", dir_num, file_num);
    return;
  }
  SDController::lockSD();

  FileEntry fe; DirEntry dir;
  if (!SDController::readFileEntry(dir_num, file_num, &fe)) {
    SDController::unlockSD();
    return;
  }
  if (!SDController::readDirEntry(dir_num, &dir)) {
    SDController::unlockSD();
    return;
  }

  if (fe.score > 0) {
    if (dir.totalScore >= fe.score) dir.totalScore -= fe.score;
    if (dir.fileCount  > 0)         dir.fileCount--;
    fe.score = 0;
    SDController::writeFileEntry(dir_num, file_num, &fe);
    SDController::writeDirEntry(dir_num, &dir);
  }
  SDController::unlockSD();
}

void SDVoting::deleteIndexedFile(uint8_t dir_num, uint8_t file_num) {
  if (AlertState::isSdBusy()) {
    PF("[SDVoting] Busy while deleting %03u/%03u\n", dir_num, file_num);
    return;
  }
  SDController::lockSD();

  FileEntry fe; DirEntry dir;
  if (!SDController::readFileEntry(dir_num, file_num, &fe)) {
    SDController::unlockSD();
    return;
  }
  if (!SDController::readDirEntry(dir_num, &dir)) {
    SDController::unlockSD();
    return;
  }

  if (fe.score > 0) {
    if (dir.totalScore >= fe.score) dir.totalScore -= fe.score;
    if (dir.fileCount  > 0)         dir.fileCount--;
  }
  fe.score   = 0;
  fe.sizeKb = 0;
  SDController::writeFileEntry(dir_num, file_num, &fe);
  SDController::writeDirEntry(dir_num, &dir);

  char path[64];
  snprintf(path, sizeof(path), "/%03u/%03u.mp3", dir_num, file_num);
  SD.remove(path);
  SDController::unlockSD();
}

bool SDVoting::getCurrentPlayable(uint8_t& d, uint8_t& f) {
  uint8_t s;
  return getCurrentDirFile(d, f, s);
}

void SDVoting::saveAccumulatedVotes() {
  if (totalVote == 0) return;
  uint8_t d = 0, f = 0, s = 0;
  if (!getCurrentDirFile(d, f, s)) { totalVote = 0; return; }
  int8_t clamped = static_cast<int8_t>(MathUtils::clamp(static_cast<int16_t>(totalVote), static_cast<int16_t>(-100), static_cast<int16_t>(100)));
  uint8_t newScore = applyVote(d, f, clamped);
  totalVote = 0;
  if (newScore > 0) {
    WebGuiStatus::setFragmentScore(newScore);
  }
}

void SDVoting::attachVoteRoute(AsyncWebServer& server) {
  server.on("/vote", HTTP_ANY, [](AsyncWebServerRequest* req) {
    const bool doDel =
      req->hasParam("del") || req->hasParam("delete") ||
      req->hasParam("del", true) || req->hasParam("delete", true);

    const bool doBan =
      req->hasParam("ban") || req->hasParam("ban", true);

    int delta = 1;
    if (!doDel && !doBan) {
      if (req->hasParam("delta"))            delta = req->getParam("delta")->value().toInt();
      else if (req->hasParam("delta", true)) delta = req->getParam("delta", true)->value().toInt();
      if (delta > 10)  delta = 10;
      if (delta < -10) delta = -10;
    }

    uint8_t dir = 0, file = 0; bool have = false;
    auto getDF = [&](bool body){
      if (req->hasParam("dir", body) && req->hasParam("file", body)) {
        long d = req->getParam("dir",  body)->value().toInt();
        long f = req->getParam("file", body)->value().toInt();
        if (d >= 1 && d <= 255 && f >= 1 && f <= 255) { dir = (uint8_t)d; file = (uint8_t)f; have = true; }
      }
    };
    getDF(false); if (!have) getDF(true);
    if (!have && !SDVoting::getCurrentPlayable(dir, file)) {
      req->send(400, "text/plain", "no current playable; supply dir & file");
      return;
    }

    if (doDel) {
      PF("[WEB] DELETE requested dir=%u file=%u\n", dir, file);
      ContextController_post(ContextController::WebCmd::DeleteFile, dir, file);
      char b1[64]; snprintf(b1, sizeof(b1), "DELETE scheduled dir=%u file=%u", dir, file);
      req->send(200, "text/plain", b1);
      return;
    }

    if (doBan) {
      PF("[WEB] BAN requested dir=%u file=%u\n", dir, file);
      ContextController_post(ContextController::WebCmd::BanFile, dir, file);
      char b2[64]; snprintf(b2, sizeof(b2), "BAN queued dir=%u file=%u", dir, file);
      req->send(200, "text/plain", b2);
      return;
    }

    if (delta != 0) {
      totalVote += delta;

      // Try to save now if SD is free, otherwise it waits for saveAccumulatedVotes()
      if (!AlertState::isSdBusy()) {
        saveAccumulatedVotes();
      }

      // Respond with predicted score
      uint8_t d2, f2, baseScore;
      getCurrentDirFile(d2, f2, baseScore);
      int16_t predicted = MathUtils::clamp(
          static_cast<int16_t>(baseScore) + static_cast<int16_t>(totalVote),
          static_cast<int16_t>(1), static_cast<int16_t>(200));
      char b3[80];
      snprintf(b3, sizeof(b3), "VOTE dir=%u file=%u delta=%d score=%u", dir, file, delta, static_cast<uint8_t>(predicted));
      req->send(200, "text/plain", b3);
    } else {
      // Score query only (delta=0): read score, indicate if unavailable
      uint8_t score = 0;
      char b3[80];
      if (readCurrentScore(dir, file, score)) {
        snprintf(b3, sizeof(b3), "SCORE dir=%u file=%u score=%u", dir, file, score);
      } else {
        snprintf(b3, sizeof(b3), "SCORE dir=%u file=%u score=?", dir, file);
      }
      req->send(200, "text/plain", b3);
    }
  });
}
