/*
 * Kwal - State module
 * Tracks modified state for colors and patterns
 */
Kwal.state = (function() {
  'use strict';

  var colorsModified = false;
  var patternModified = false;
  
  // Original values to revert to
  var originalColors = { a: null, b: null, id: null };
  var originalPattern = { id: null };

  function setColorsModified(modified, colorA, colorB, id) {
    colorsModified = modified;
    if (modified && colorA && colorB) {
      originalColors = { a: colorA, b: colorB, id: id };
    }
    updateDevModal();
  }

  function setPatternModified(modified, id) {
    patternModified = modified;
    if (modified && id) {
      originalPattern = { id: id };
    }
    updateDevModal();
  }

  function isColorsModified() { return colorsModified; }
  function isPatternModified() { return patternModified; }
  function getOriginalColors() { return originalColors; }
  function getOriginalPattern() { return originalPattern; }

  function clearColorsModified() {
    colorsModified = false;
    originalColors = { a: null, b: null, id: null };
    updateDevModal();
  }

  function clearPatternModified() {
    patternModified = false;
    originalPattern = { id: null };
    updateDevModal();
  }

  function updateDevModal() {
    var saveColorsBtn = document.getElementById('save-colors-btn');
    var savePatternBtn = document.getElementById('save-pattern-btn');
    if (saveColorsBtn) {
      saveColorsBtn.style.display = colorsModified ? 'block' : 'none';
    }
    if (savePatternBtn) {
      savePatternBtn.style.display = patternModified ? 'block' : 'none';
    }
  }

  return {
    setColorsModified: setColorsModified,
    setPatternModified: setPatternModified,
    isColorsModified: isColorsModified,
    isPatternModified: isPatternModified,
    getOriginalColors: getOriginalColors,
    getOriginalPattern: getOriginalPattern,
    clearColorsModified: clearColorsModified,
    clearPatternModified: clearPatternModified
  };
})();
