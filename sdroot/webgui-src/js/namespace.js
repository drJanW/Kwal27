/*
 * Kwal - Global namespace
 */
var Kwal = Kwal || {};
window.KWAL_JS_VERSION = '{{VERSION}}';  // Injected by build.ps1

/**
 * Logarithmic slider mapping (power curve).
 * Gives fine control at the low end, coarser at the high end.
 * Exponent 2 (quadratic) — perceptually natural for brightness, volume, speed.
 *
 * sliderToValue(pos, min, max) — slider position → real value
 * valueToSlider(val, min, max) — real value → slider position (for SSE updates)
 */
Kwal.LOG_EXP = 2;

Kwal.sliderToValue = function(pos, min, max) {
  var fraction = (pos - min) / (max - min);       // 0..1
  var curved = Math.pow(fraction, Kwal.LOG_EXP);   // apply curve
  return min + curved * (max - min);
};

Kwal.valueToSlider = function(val, min, max) {
  var fraction = (val - min) / (max - min);         // 0..1
  var linear = Math.pow(fraction, 1 / Kwal.LOG_EXP); // invert curve
  return min + linear * (max - min);
};
