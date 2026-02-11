/**
 * @file SDVoting.h
 * @brief Audio fragment voting system interface with score tracking per file
 * @version 260202A
 $12026-02-10
 */
#pragma once
#include <Arduino.h>
#include "SDController.h"

class AsyncWebServer;

namespace SDVoting {
  uint8_t getRandomFile(uint8_t dir_num);
  uint8_t applyVote(uint8_t dir_num, uint8_t file_num, int8_t delta);  // Returns new score, 0 on failure
  void    saveAccumulatedVotes();   // Write totalVote to SD and reset to 0
  void    banFile(uint8_t dir_num, uint8_t file_num);
  void    deleteIndexedFile(uint8_t dir_num, uint8_t file_num);
  bool    getCurrentPlayable(uint8_t& dirOut, uint8_t& fileOut);
  void    attachVoteRoute(AsyncWebServer& server);
}
