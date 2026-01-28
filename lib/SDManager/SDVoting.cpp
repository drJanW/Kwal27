/**
 * @file SDVoting.cpp
 * @brief Audio fragment voting system implementation with score tracking per file
 * @version 251231E
 * @date 2025-12-31
 *
 * Implements a voting system for audio fragments stored on SD card.
 * Features:
 * - Score tracking per file (1-200 range, 0=empty/banned)
 * - Weighted random file selection based on scores
 * - Vote application with delta adjustments (+/-)
 * - File banning (score set to 0) and deletion
 * - Web API route for vote submission via AsyncWebServer
 * - Integration with AudioState for tracking currently playing file
 * - Persistent score storage in the SD card index files
 */

#include <Arduino.h>
#include "SDVoting.h"
#include "Globals.h"
#include "MathUtils.h"
#include "AudioState.h"
#include "ContextManager.h"
#include "SDManager.h"
#include "WebGuiStatus.h"

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

bool readCurrentScore(uint8_t dir, uint8_t file, uint8_t& scoreOut) {
  if (SDManager::isSDbusy()) {
    PF("[SDVoting] Busy while reading score %03u/%03u\n", dir, file);
    scoreOut = 0;
    return false;
  }
  SDManager::lockSD();

  FileEntry fe;
  if (!SDManager::readFileEntry(dir, file, &fe)) {
    scoreOut = 0;
    SDManager::unlockSD();
    return false;
  }
  scoreOut = fe.score;
  SDManager::unlockSD();
  return true;
}

} // namespace

uint8_t SDVoting::getRandomFile(uint8_t dir_num) {
  if (SDManager::isSDbusy()) {
    PF("[SDVoting] Busy while selecting file from dir %03u\n", dir_num);
    return 0;
  }
  SDManager::lockSD();

  DirEntry dir;
  if (!SDManager::readDirEntry(dir_num, &dir) || dir.fileCount == 0) {
    SDManager::unlockSD();
    return 0;
  }

  uint8_t  fileNums[SD_MAX_FILES_PER_SUBDIR];
  uint8_t  fileScores[SD_MAX_FILES_PER_SUBDIR];
  uint8_t  count = 0;
  uint16_t total = 0;

  for (uint8_t i = 1; i <= SD_MAX_FILES_PER_SUBDIR; i++) {
    FileEntry fe;
  if (SDManager::readFileEntry(dir_num, i, &fe) && fe.score > 0) {
      fileNums[count]   = i;
      fileScores[count] = fe.score;
      total += fe.score;
      if (++count >= dir.fileCount) break;
    }
  }
  if (count == 0) {
    SDManager::unlockSD();
    return 0;
  }

  uint16_t pick = random(1, total + 1), acc = 0;
  for (uint8_t i = 0; i < count; i++) {
    acc += fileScores[i];
    if (pick <= acc) {
      SDManager::unlockSD();
      return fileNums[i];
    }
  }
  SDManager::unlockSD();
  return fileNums[0];
}

uint8_t SDVoting::applyVote(uint8_t dir_num, uint8_t file_num, int8_t delta) {
  // NOTE: Don't check isSDbusy() here - voting should work during playback
  // The SD card can handle interleaved small reads/writes during MP3 streaming

  FileEntry fe; DirEntry dir;
  if (!SDManager::readFileEntry(dir_num, file_num, &fe)) {
    return 0;
  }
  if (!SDManager::readDirEntry(dir_num, &dir)) {
    return 0;
  }
  if (fe.score == 0) {
    return 0;
  }

  delta = MathUtils::clamp(delta, (int8_t)-10, (int8_t)10);

  int16_t ns = static_cast<int16_t>(fe.score) + static_cast<int16_t>(delta);
  ns = MathUtils::clamp(ns, static_cast<int16_t>(1), static_cast<int16_t>(200));

  dir.totalScore += (ns - fe.score);
  fe.score = (uint8_t)ns;

  SDManager::writeFileEntry(dir_num, file_num, &fe);
  SDManager::writeDirEntry(dir_num, &dir);
  
  return fe.score;  // Return the new score
}

void SDVoting::banFile(uint8_t dir_num, uint8_t file_num) {
  if (SDManager::isSDbusy()) {
    PF("[SDVoting] Busy while banning %03u/%03u\n", dir_num, file_num);
    return;
  }
  SDManager::lockSD();

  FileEntry fe; DirEntry dir;
  if (!SDManager::readFileEntry(dir_num, file_num, &fe)) {
    SDManager::unlockSD();
    return;
  }
  if (!SDManager::readDirEntry(dir_num, &dir)) {
    SDManager::unlockSD();
    return;
  }

  if (fe.score > 0) {
    if (dir.totalScore >= fe.score) dir.totalScore -= fe.score;
    if (dir.fileCount  > 0)         dir.fileCount--;
    fe.score = 0;
    SDManager::writeFileEntry(dir_num, file_num, &fe);
    SDManager::writeDirEntry(dir_num, &dir);
  }
  SDManager::unlockSD();
}

void SDVoting::deleteIndexedFile(uint8_t dir_num, uint8_t file_num) {
  if (SDManager::isSDbusy()) {
    PF("[SDVoting] Busy while deleting %03u/%03u\n", dir_num, file_num);
    return;
  }
  SDManager::lockSD();

  FileEntry fe; DirEntry dir;
  if (!SDManager::readFileEntry(dir_num, file_num, &fe)) {
    SDManager::unlockSD();
    return;
  }
  if (!SDManager::readDirEntry(dir_num, &dir)) {
    SDManager::unlockSD();
    return;
  }

  if (fe.score > 0) {
    if (dir.totalScore >= fe.score) dir.totalScore -= fe.score;
    if (dir.fileCount  > 0)         dir.fileCount--;
  }
  fe.score   = 0;
  fe.sizeKb = 0;
  SDManager::writeFileEntry(dir_num, file_num, &fe);
  SDManager::writeDirEntry(dir_num, &dir);

  char path[64];
  snprintf(path, sizeof(path), "/%03u/%03u.mp3", dir_num, file_num);
  SD.remove(path);
  SDManager::unlockSD();
}

bool SDVoting::getCurrentPlayable(uint8_t& d, uint8_t& f) {
  uint8_t s;
  return getCurrentDirFile(d, f, s);
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
      ContextManager_post(ContextManager::WebCmd::DeleteFile, dir, file);
      char b1[64]; snprintf(b1, sizeof(b1), "DELETE scheduled dir=%u file=%u", dir, file);
      req->send(200, "text/plain", b1);
      return;
    }

    if (doBan) {
      PF("[WEB] BAN requested dir=%u file=%u\n", dir, file);
      ContextManager_post(ContextManager::WebCmd::BanFile, dir, file);
      char b2[64]; snprintf(b2, sizeof(b2), "BAN queued dir=%u file=%u", dir, file);
      req->send(200, "text/plain", b2);
      return;
    }

    PF("[WEB] VOTE requested dir=%u file=%u delta=%d\n", dir, file, delta);
    
    if (delta != 0) {
      // Apply vote directly (bypasses ContextManager queue for immediate response)
      uint8_t newScore = SDVoting::applyVote(dir, file, delta);
      if (newScore > 0) {
        WebGuiStatus::setFragmentScore(newScore);
      }
      char b3[80]; 
      if (newScore > 0) {
        snprintf(b3, sizeof(b3), "VOTE applied dir=%u file=%u delta=%d score=%u", dir, file, delta, newScore);
      } else {
        snprintf(b3, sizeof(b3), "VOTE failed dir=%u file=%u delta=%d score=?", dir, file, delta);
      }
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
