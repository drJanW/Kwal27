/**
 * @file SDVoting.h
 * @brief Audio fragment voting system interface with score tracking per file
 * @version 251231E
 * @date 2025-12-31
 *
 * Defines the SDVoting namespace for managing audio file scores.
 * Provides functions for:
 * - getRandomFile: Weighted random selection based on file scores
 * - applyVote: Adjust file score by delta value
 * - banFile: Set file score to 0 (excluded from playback)
 * - deleteIndexedFile: Remove file from index
 * - getCurrentPlayable: Get currently playing file info
 * - attachVoteRoute: Register web API endpoint for voting
 */

#pragma once
#include <Arduino.h>
#include "SDController.h"

class AsyncWebServer;

namespace SDVoting {
  uint8_t getRandomFile(uint8_t dir_num);
  uint8_t applyVote(uint8_t dir_num, uint8_t file_num, int8_t delta);  // Returns new score, 0 on failure
  void    banFile(uint8_t dir_num, uint8_t file_num);
  void    deleteIndexedFile(uint8_t dir_num, uint8_t file_num);
  bool    getCurrentPlayable(uint8_t& dirOut, uint8_t& fileOut);
  void    attachVoteRoute(AsyncWebServer& server);
}
