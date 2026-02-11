/*
 * Kwal - Brightness module
 * See docs/glossary_slider_semantics.md for terminology
 * 
 * Slider shows sliderPct (0-100%) directly.
 * Grey zone left: 0% to loPct (below minimum)
 * Green zone: loPct to hiPct (usable range)
 * 
 * sliderPct = current brightness as percentage of Lo..Hi range
 */
Kwal.brightness = (function() {
  'use strict';

  var slider, label;
  var loPct = 28;     // Left grey zone boundary
  var hiPct = 100;    // Right grey zone boundary

  function clamp(val) {
    return Math.max(loPct, Math.min(hiPct, val));
  }

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
      var pos = clamp(parseInt(slider.value, 10));
      slider.value = pos;
      label.textContent = pos + '%';
    };

    slider.onchange = function() {
      var pos = clamp(parseInt(slider.value, 10));
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
      var pos = clamp(Math.round(sliderPct));
      slider.value = pos;
      label.textContent = pos + '%';
    }
  }

  return { init: init, updateFromState: updateFromState };
})();
