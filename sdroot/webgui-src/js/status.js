/*
 * Kwal - Status module
 * Shows current active pattern and colors on main screen
 * Labels now come from SSE state event (patternLabel, colorLabel)
 */
Kwal.status = (function() {
  'use strict';

  var patternLabel, colorsLabel;

  function init() {
    patternLabel = document.getElementById('current-pattern');
    colorsLabel = document.getElementById('current-colors');
    // No load() - labels come from SSE state event
  }

  /**
   * Update labels from SSE state event
   * @param {string} pattern - Pattern label
   * @param {string} color - Color label
   */
  function updateFromState(pattern, color) {
    if (patternLabel) {
      patternLabel.textContent = pattern || '-';
    }
    if (colorsLabel) {
      colorsLabel.textContent = color || '-';
    }
  }

  // Keep load() for backward compatibility but it does nothing now
  function load() {}

  return { init: init, load: load, updateFromState: updateFromState };
})();
