/*
 * Kwal - Custom Color Picker module
 * Canvas-based HSV picker that works on all browsers including
 * Android WebView (DuckDuckGo, etc.) where <input type="color"> fails.
 *
 * API:
 *   Kwal.colorpicker.open(currentHex, onSelect)
 *     currentHex: '#rrggbb' initial color
 *     onSelect:   function(hex) called on OK with chosen '#rrggbb'
 */
Kwal.colorpicker = (function() {
  'use strict';

  var modal, svCanvas, svCtx, hueCanvas, hueCtx;
  var hexInput, previewEl, okBtn, cancelBtn;
  var onSelectCb = null;

  // Current HSV state (h: 0-360, s: 0-1, v: 0-1)
  var curH = 0, curS = 1, curV = 1;

  var SV_SIZE = 200;   // saturation-value square side
  var HUE_W = 200;     // hue bar width (matches SV)
  var HUE_H = 24;      // hue bar height

  var draggingSV = false;
  var draggingHue = false;

  function init() {
    modal = document.getElementById('colorpicker-modal');
    if (!modal) return;

    svCanvas   = document.getElementById('cp-sv');
    hueCanvas  = document.getElementById('cp-hue');
    hexInput   = document.getElementById('cp-hex');
    previewEl  = document.getElementById('cp-preview');
    okBtn      = document.getElementById('cp-ok');
    cancelBtn  = document.getElementById('cp-cancel');

    if (!svCanvas || !hueCanvas) return;

    // Size canvases
    svCanvas.width  = SV_SIZE;
    svCanvas.height = SV_SIZE;
    hueCanvas.width = HUE_W;
    hueCanvas.height = HUE_H;

    svCtx  = svCanvas.getContext('2d');
    hueCtx = hueCanvas.getContext('2d');

    // --- SV canvas events (mouse + touch) ---
    svCanvas.addEventListener('mousedown', function(e) { draggingSV = true; pickSV(e); });
    document.addEventListener('mousemove', function(e) { if (draggingSV) pickSV(e); });
    document.addEventListener('mouseup', function() { draggingSV = false; });

    svCanvas.addEventListener('touchstart', function(e) {
      e.preventDefault(); draggingSV = true; pickSV(e.touches[0]);
    }, { passive: false });
    svCanvas.addEventListener('touchmove', function(e) {
      e.preventDefault(); if (draggingSV) pickSV(e.touches[0]);
    }, { passive: false });
    svCanvas.addEventListener('touchend', function() { draggingSV = false; });

    // --- Hue bar events ---
    hueCanvas.addEventListener('mousedown', function(e) { draggingHue = true; pickHue(e); });
    document.addEventListener('mousemove', function(e) { if (draggingHue) pickHue(e); });
    document.addEventListener('mouseup', function() { draggingHue = false; });

    hueCanvas.addEventListener('touchstart', function(e) {
      e.preventDefault(); draggingHue = true; pickHue(e.touches[0]);
    }, { passive: false });
    hueCanvas.addEventListener('touchmove', function(e) {
      e.preventDefault(); if (draggingHue) pickHue(e.touches[0]);
    }, { passive: false });
    hueCanvas.addEventListener('touchend', function() { draggingHue = false; });

    // Hex input
    if (hexInput) {
      hexInput.addEventListener('input', function() {
        var v = hexInput.value.trim();
        if (/^#[0-9a-fA-F]{6}$/.test(v)) {
          var hsv = hexToHSV(v);
          curH = hsv.h; curS = hsv.s; curV = hsv.v;
          drawAll();
        }
      });
    }

    // OK / Cancel
    if (okBtn) okBtn.onclick = function() {
      if (onSelectCb) onSelectCb(hsvToHex(curH, curS, curV));
      closeModal();
    };
    if (cancelBtn) cancelBtn.onclick = function() { closeModal(); };

    // Backdrop click closes
    modal.onclick = function(e) { if (e.target === modal) closeModal(); };
  }

  function open(hex, onSelect) {
    if (!modal) return;
    onSelectCb = onSelect;
    var hsv = hexToHSV(hex || '#ff0000');
    curH = hsv.h; curS = hsv.s; curV = hsv.v;
    modal.classList.add('open');
    drawAll();
  }

  function closeModal() {
    if (modal) modal.classList.remove('open');
    onSelectCb = null;
    draggingSV = false;
    draggingHue = false;
  }

  // --- Drawing ---

  function drawAll() {
    drawSV();
    drawHue();
    updatePreview();
  }

  function drawSV() {
    if (!svCtx) return;
    var w = SV_SIZE, h = SV_SIZE;

    // Base hue fill
    svCtx.fillStyle = 'hsl(' + curH + ',100%,50%)';
    svCtx.fillRect(0, 0, w, h);

    // White gradient left-to-right (saturation)
    var gWhite = svCtx.createLinearGradient(0, 0, w, 0);
    gWhite.addColorStop(0, 'rgba(255,255,255,1)');
    gWhite.addColorStop(1, 'rgba(255,255,255,0)');
    svCtx.fillStyle = gWhite;
    svCtx.fillRect(0, 0, w, h);

    // Black gradient top-to-bottom (value)
    var gBlack = svCtx.createLinearGradient(0, 0, 0, h);
    gBlack.addColorStop(0, 'rgba(0,0,0,0)');
    gBlack.addColorStop(1, 'rgba(0,0,0,1)');
    svCtx.fillStyle = gBlack;
    svCtx.fillRect(0, 0, w, h);

    // Crosshair
    var cx = curS * w;
    var cy = (1 - curV) * h;
    svCtx.beginPath();
    svCtx.arc(cx, cy, 6, 0, Math.PI * 2);
    svCtx.strokeStyle = '#fff';
    svCtx.lineWidth = 2;
    svCtx.stroke();
    svCtx.beginPath();
    svCtx.arc(cx, cy, 7, 0, Math.PI * 2);
    svCtx.strokeStyle = '#000';
    svCtx.lineWidth = 1;
    svCtx.stroke();
  }

  function drawHue() {
    if (!hueCtx) return;
    var w = HUE_W, h = HUE_H;
    var grad = hueCtx.createLinearGradient(0, 0, w, 0);
    for (var i = 0; i <= 6; i++) {
      grad.addColorStop(i / 6, 'hsl(' + (i * 60) + ',100%,50%)');
    }
    hueCtx.fillStyle = grad;
    hueCtx.fillRect(0, 0, w, h);

    // Indicator
    var ix = (curH / 360) * w;
    hueCtx.beginPath();
    hueCtx.rect(ix - 3, 0, 6, h);
    hueCtx.strokeStyle = '#fff';
    hueCtx.lineWidth = 2;
    hueCtx.stroke();
    hueCtx.beginPath();
    hueCtx.rect(ix - 4, 0, 8, h);
    hueCtx.strokeStyle = '#000';
    hueCtx.lineWidth = 1;
    hueCtx.stroke();
  }

  function updatePreview() {
    var hex = hsvToHex(curH, curS, curV);
    if (previewEl) previewEl.style.background = hex;
    if (hexInput) hexInput.value = hex;
  }

  // --- Coordinate picking ---

  function pickSV(e) {
    var r = svCanvas.getBoundingClientRect();
    var x = (e.clientX - r.left);
    var y = (e.clientY - r.top);
    curS = Math.max(0, Math.min(1, x / SV_SIZE));
    curV = Math.max(0, Math.min(1, 1 - y / SV_SIZE));
    drawAll();
  }

  function pickHue(e) {
    var r = hueCanvas.getBoundingClientRect();
    var x = (e.clientX - r.left);
    curH = Math.max(0, Math.min(360, (x / HUE_W) * 360));
    drawAll();
  }

  // --- Color conversion ---

  function hsvToRGB(h, s, v) {
    var c = v * s;
    var x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    var m = v - c;
    var r = 0, g = 0, b = 0;
    if      (h < 60)  { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else              { r = c; b = x; }
    return {
      r: Math.round((r + m) * 255),
      g: Math.round((g + m) * 255),
      b: Math.round((b + m) * 255)
    };
  }

  function hsvToHex(h, s, v) {
    var rgb = hsvToRGB(h, s, v);
    return '#' + pad2(rgb.r) + pad2(rgb.g) + pad2(rgb.b);
  }

  function hexToHSV(hex) {
    var r = parseInt(hex.substr(1, 2), 16) / 255;
    var g = parseInt(hex.substr(3, 2), 16) / 255;
    var b = parseInt(hex.substr(5, 2), 16) / 255;
    var max = Math.max(r, g, b), min = Math.min(r, g, b);
    var d = max - min;
    var h = 0, s = max === 0 ? 0 : d / max, v = max;
    if (d !== 0) {
      if (max === r)      h = 60 * (((g - b) / d) % 6);
      else if (max === g) h = 60 * ((b - r) / d + 2);
      else                h = 60 * ((r - g) / d + 4);
      if (h < 0) h += 360;
    }
    return { h: h, s: s, v: v };
  }

  function pad2(n) {
    var s = n.toString(16);
    return s.length < 2 ? '0' + s : s;
  }

  return { init: init, open: open };
})();
