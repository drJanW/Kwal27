/*
 * Kwal - Brightness module
 * See docs/glossary_slider_semantics.md for terminology
 * 
 * Slider moves freely 0-100%. Grey zones show shiftedLo/Hi
 * as visual indicators but do NOT restrict the thumb.
 * 
 * sliderPct = current brightness as percentage of Lo..Hi range
 */
Kwal.brightness = (function() {
  'use strict';

  var slider, label;
  var pctMin = 0;     // Slider minimum
  var pctMax = 100;   // Slider maximum
  var loPct = 28;     // Grey zone left boundary (visual only)
  var hiPct = 100;    // Grey zone right boundary (visual only)

  function updateGradient() {
    if (!slider) return;
    // Grey | Green | Grey
    var style = 'linear-gradient(to right, ' +
      '#555 0%, #555 ' + loPct + '%, ' +
      '#4CAF50 ' + loPct + '%, #4CAF50 ' + hiPct + '%, ' +
      '#555 ' + hiPct + '%, #555 100%)';
    slider.style.background = style;
  }

  function init() {
    slider = document.getElementById('brightness');
    label = document.getElementById('bri-num');
    
    if (!slider || !label) return;

    slider.oninput = function() {
      var pos = Math.max(pctMin, Math.min(pctMax, parseInt(slider.value, 10)));
      slider.value = pos;
      label.textContent = pos + '%';
    };

    slider.onchange = function() {
      var pos = Math.max(pctMin, Math.min(pctMax, parseInt(slider.value, 10)));
      slider.value = pos;
      label.textContent = pos + '%';
      fetch('/setBrightness?value=' + pos, { method: 'POST' }).catch(function() {});
    };
    
    updateGradient();
  }

  /**
   * Update brightness from SSE state event
   * @param {number} sliderPct Current brightness as percentage (0-100)
   * @param {number} loPercent Left grey zone boundary (%)
   * @param {number} hiPercent Right grey zone boundary (%)
   */
  function updateFromState(sliderPct, loPercent, hiPercent) {
    if (typeof loPercent === 'number') loPct = loPercent;
    if (typeof hiPercent === 'number') hiPct = hiPercent;
    updateGradient();
    if (slider && label && typeof sliderPct === 'number') {
      var pos = Math.max(pctMin, Math.min(pctMax, Math.round(sliderPct)));
      slider.value = pos;
      label.textContent = pos + '%';
    }
  }

  return { init: init, updateFromState: updateFromState };
})();
