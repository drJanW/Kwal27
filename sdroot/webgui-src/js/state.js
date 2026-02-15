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
    var saveColorsBar = document.getElementById('save-colors-bar');
    var savePatternBar = document.getElementById('save-pattern-bar');
    if (saveColorsBar) {
      saveColorsBar.style.display = colorsModified ? 'flex' : 'none';
    }
    if (savePatternBar) {
      savePatternBar.style.display = patternModified ? 'flex' : 'none';
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
