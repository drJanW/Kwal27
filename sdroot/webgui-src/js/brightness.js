/**
 * @file    brightness.js
 * @version 260316B
 * @date    2026-03-16
 *
 * Kwal - Brightness module
 * See docs/glossary_slider_semantics.md for terminology
 *
 * Slider moves freely 0-100%. Grey zones show shiftedLo/Hi
 * as visual indicators but do NOT restrict the thumb.
 * 
 * sliderPct = current brightness as percentage of Lo..Hi range
 *
 * Dark mode: click lightbulb to toggle LEDs off (brightness=0).
 * While dark, SSE brightness updates are blocked so the LEDs stay off.
 */
Kwal.brightness = (function() {
  'use strict';

  var slider, label, dmBtn;
  var pctMin = 0;     // Slider minimum
  var pctMax = 100;   // Slider maximum
  var loPct = 28;     // Grey zone left boundary (visual only)
  var hiPct = 100;    // Grey zone right boundary (visual only)
  var dark = false;           // LED dark mode active
  var savedBrightness = 50;   // Brightness before dark mode

  function updateGradient() {
    if (!slider) return;
    // Grey | Green | Grey
    var style = 'linear-gradient(to right, ' +
      '#555 0%, #555 ' + loPct + '%, ' +
      '#4CAF50 ' + loPct + '%, #4CAF50 ' + hiPct + '%, ' +
      '#555 ' + hiPct + '%, #555 100%)';
    slider.style.background = style;
  }

  function setDarkVisual(on) {
    if (dmBtn) dmBtn.style.opacity = on ? '0.3' : '1';
  }

  function init() {
    slider = document.getElementById('brightness');
    label = document.getElementById('bri-num');
    dmBtn = document.getElementById('darkmode-toggle');
    
    if (!slider || !label) return;

    slider.oninput = function() {
      var pos = Math.max(pctMin, Math.min(pctMax, parseInt(slider.value, 10)));
      slider.value = pos;
      label.textContent = pos + '%';
    };

    slider.onchange = function() {
      if (dark) { dark = false; setDarkVisual(false); }
      var pos = Math.max(pctMin, Math.min(pctMax, parseInt(slider.value, 10)));
      slider.value = pos;
      label.textContent = pos + '%';
      fetch('/setBrightness?value=' + pos, { method: 'POST' }).catch(function() {});
    };

    // Dark mode: click lightbulb to toggle LEDs off/on
    if (dmBtn) {
      dmBtn.onclick = function() {
        if (!dark) {
          savedBrightness = parseInt(slider.value, 10) || 50;
          dark = true;
          setDarkVisual(true);
          slider.value = 0;
          label.textContent = '0%';
          fetch('/setBrightness?value=0&dark=1&was=' + savedBrightness, { method: 'POST' }).catch(function() {});
        } else {
          dark = false;
          setDarkVisual(false);
          slider.value = savedBrightness;
          label.textContent = savedBrightness + '%';
          fetch('/setBrightness?value=' + savedBrightness + '&dark=0', { method: 'POST' }).catch(function() {});
        }
      };
    }
    
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
    if (dark) return;  // Block SSE overwrite while LEDs are off
    if (slider && label && typeof sliderPct === 'number') {
      var pos = Math.max(pctMin, Math.min(pctMax, Math.round(sliderPct)));
      slider.value = pos;
      label.textContent = pos + '%';
    }
  }

  return { init: init, updateFromState: updateFromState };
})();
