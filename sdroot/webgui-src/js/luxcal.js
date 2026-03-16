/**
 * @file    luxcal.js
 * @version 260316K
 * @date    2026-03-16
 *
 * Kwal - Lux Calibration module
 * Calibration panel for lux → brightness curve fitting
 * API: POST /api/lux/calibrate, /api/lux/sample, /api/lux/status,
 *       /api/lux/fit, /api/lux/clear, /api/lux/reload, GET /api/lux/csv
 */
Kwal.luxcal = (function() {
  'use strict';

  var sampleBtn, fitBtn, clearBtn, downloadBtn, acceptBtn;
  var statusEl, sampleCountEl, luxValueEl, brightnessValueEl, fitResultEl;
  var newFitEl, newParamsEl;
  var briSlider, briLabel;
  var chartCanvas;
  var savedBrightness = -1;  // brightness before modal open (-1 = not saved)
  var hasLuxSensor = true;   // assume present until status says otherwise

  // Quadratic slider mapping: more resolution at low brightness
  function sliderToBrightness(v) { return Math.round(v * v / 100); }
  function brightnessToSlider(b) { return Math.round(Math.sqrt(b * 100)); }

  function formatParams(d) {
    var s = 'brMax=' + (d.brMax != null ? Number(d.brMax).toFixed(1) : '?') +
           ' rate=' + (d.luxRate != null ? Number(d.luxRate).toFixed(4) : '?');
    if (d.luxMax != null) s += ' (lux 0-' + Number(d.luxMax).toFixed(0) + ')';
    return s;
  }

  function setStatus(text) {
    if (statusEl) statusEl.textContent = text;
  }

  function showCount(real) {
    if (sampleCountEl) sampleCountEl.textContent = real + ' samples';
  }

  function hideAccept() {
    if (newFitEl) newFitEl.style.display = 'none';
  }

  var lastFitParams = null;

  function setDisabledAll(disabled) {
    if (sampleBtn)   sampleBtn.disabled = disabled;
    if (fitBtn)      fitBtn.disabled = disabled;
    if (clearBtn)    clearBtn.disabled = disabled;
    if (downloadBtn) downloadBtn.disabled = disabled;
    if (briSlider)   briSlider.disabled = disabled;
  }

  function updateUI(data) {
    if (!data) return;
    showCount(data.realCount || 0);
    if (typeof data.lastLux === 'number' && luxValueEl) {
      luxValueEl.textContent = data.lastLux.toFixed(1);
    }
    if (typeof data.lastBrightness === 'number' && brightnessValueEl) {
      brightnessValueEl.textContent = data.lastBrightness.toFixed(1);
    }
  }

  function loadStatus() {
    // Save current brightness and sync cal slider
    var mainSlider = document.getElementById('brightness');
    if (mainSlider && briSlider && briLabel) {
      savedBrightness = parseInt(mainSlider.value, 10);
      briSlider.value = brightnessToSlider(savedBrightness);
      briLabel.textContent = savedBrightness + '%';
    }
    fetch('/api/lux/status').then(function(r) { return r.json(); })
      .then(function(data) {
        hasLuxSensor = !!data.hasLuxSensor;
        if (!hasLuxSensor) {
          setDisabledAll(true);
          setStatus('Geen lux sensor');
          return;
        }
        updateUI(data);
        // Show current params
        if (fitResultEl && typeof data.brMax === 'number') {
          fitResultEl.textContent = 'Huidig: ' + formatParams(data);
        }
        hideAccept();
        // Auto-enable calibration mode on modal open
        fetch('/api/lux/calibrate?mode=on', { method: 'POST' })
          .then(function(r) { return r.json(); })
          .then(function(d) {
            updateUI(d);
            if (sampleBtn) sampleBtn.disabled = false;
            fetchAndDraw();
          })
          .catch(function() { setStatus('Calibratie activeren mislukt'); });
      })
      .catch(function() { setStatus('Status ophalen mislukt'); });
  }

  function onModalClose() {
    // Restore brightness to value before modal was opened
    if (savedBrightness >= 0) {
      fetch('/setBrightness?value=' + savedBrightness, { method: 'POST' }).catch(function() {});
      var mainSlider = document.getElementById('brightness');
      var mainLabel = document.getElementById('bri-num');
      if (mainSlider) mainSlider.value = savedBrightness;
      if (mainLabel) mainLabel.textContent = savedBrightness + '%';
      if (briSlider) briSlider.value = brightnessToSlider(savedBrightness);
      if (briLabel) briLabel.textContent = savedBrightness + '%';
      savedBrightness = -1;
    }
    // Auto-disable calibration mode on modal close
    fetch('/api/lux/calibrate?mode=off', { method: 'POST' }).catch(function() {});
  }

  function takeSample() {
    setStatus('Meting...');
    sampleBtn.disabled = true;
    fetch('/api/lux/sample', { method: 'POST' })
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (data.ok) {
          setStatus('Sample opgeslagen');
          showCount(data.realCount || 0);
          if (luxValueEl) luxValueEl.textContent = data.lux.toFixed(1);
          if (brightnessValueEl) brightnessValueEl.textContent = data.brightness.toFixed(1);
        } else {
          setStatus(data.error || 'Fout');
        }
        sampleBtn.disabled = false;
      })
      .catch(function() { setStatus('Sample mislukt'); sampleBtn.disabled = false; });
  }

  function doFit() {
    setStatus('Fitting...');
    fetch('/api/lux/fit', { method: 'POST' })
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (data.ok) {
          if (fitResultEl) {
            fitResultEl.textContent = 'Oud: ' + formatParams({
              brMax: data.oldBrMax, luxRate: data.oldLuxRate, luxMax: data.luxMax
            });
          }
          if (newParamsEl) {
            newParamsEl.textContent = 'Nieuw: ' + formatParams(data) +
              ' R\u00b2=' + (data.r2 != null ? Number(data.r2).toFixed(3) : '?') + ' (n=' + (data.realCount || 0) + ')';
          }
          if (newFitEl) newFitEl.style.display = 'block';          lastFitParams = { brMax: data.brMax, luxRate: data.luxRate };          setStatus('Fit berekend — accepteer of sample verder');
        } else {
          setStatus(data.error || 'Fit mislukt');
        }
      })
      .catch(function() { setStatus('Fit mislukt'); });
  }

  function doAccept() {
    setStatus('Opslaan...');
    fetch('/api/lux/accept', { method: 'POST' })
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (data.ok) {
          hideAccept();
          showCount(data.realCount || 0);
          if (fitResultEl && lastFitParams) {
            fitResultEl.textContent = 'Huidig: ' + formatParams(lastFitParams);
          }
          lastFitParams = null;
          if (sampleBtn) sampleBtn.disabled = false;
          fetchAndDraw();
          setStatus('Opgeslagen, seeds vernieuwd');
        } else {
          setStatus('Opslaan mislukt');
        }
      })
      .catch(function() { setStatus('Opslaan mislukt'); });
  }

  function doClear() {
    if (!confirm('Reset naar seeds?')) return;
    fetch('/api/lux/clear', { method: 'POST' })
      .then(function(r) { return r.json(); })
      .then(function(data) {
        showCount(0);
        if (fitResultEl) fitResultEl.textContent = '-';
        hideAccept();
        if (sampleBtn) sampleBtn.disabled = false;
        fetchAndDraw();
        setStatus('Reset naar seeds');
      })
      .catch(function() { setStatus('Reset mislukt'); });
  }

  function doDownload() {
    window.open('/api/lux/csv', '_blank');
  }

  function drawChart(data) {
    if (!chartCanvas) return;
    var ctx = chartCanvas.getContext('2d');
    var W = chartCanvas.width, H = chartCanvas.height;
    var pad = { l: 36, r: 10, t: 10, b: 22 };
    var pw = W - pad.l - pad.r, ph = H - pad.t - pad.b;

    ctx.clearRect(0, 0, W, H);

    // Determine axis ranges
    var xMax = data.luxMax || 300;
    var yMax = data.brMax || 100;
    // Expand yMax if any point exceeds it
    var pts = data.points || [];
    for (var i = 0; i < pts.length; i++) {
      if (pts[i].bri > yMax) yMax = pts[i].bri * 1.1;
    }

    // Grid lines + labels
    ctx.strokeStyle = '#333';
    ctx.lineWidth = 0.5;
    ctx.fillStyle = '#666';
    ctx.font = '9px sans-serif';
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    var ySteps = 4;
    for (var j = 0; j <= ySteps; j++) {
      var yv = yMax * j / ySteps;
      var yp = pad.t + ph - (j / ySteps) * ph;
      ctx.beginPath(); ctx.moveTo(pad.l, yp); ctx.lineTo(pad.l + pw, yp); ctx.stroke();
      ctx.fillText(Math.round(yv), pad.l - 4, yp);
    }
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    var xSteps = 4;
    for (var k = 0; k <= xSteps; k++) {
      var xv = xMax * k / xSteps;
      var xp = pad.l + (k / xSteps) * pw;
      ctx.beginPath(); ctx.moveTo(xp, pad.t); ctx.lineTo(xp, pad.t + ph); ctx.stroke();
      ctx.fillText(Math.round(xv), xp, pad.t + ph + 4);
    }

    // Axis labels
    ctx.fillStyle = '#888';
    ctx.textAlign = 'center';
    ctx.fillText('lux', pad.l + pw / 2, H - 2);
    ctx.save();
    ctx.translate(8, pad.t + ph / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText('bri', 0, 0);
    ctx.restore();

    // Fitted curve (green)
    var brMax = data.brMax || 100;
    var luxRate = data.luxRate || 0.02;
    ctx.strokeStyle = '#0c0';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    for (var cx = 0; cx <= pw; cx++) {
      var lux = (cx / pw) * xMax;
      var bri = brMax * (1.0 - Math.exp(-luxRate * lux));
      var py = pad.t + ph - (bri / yMax) * ph;
      if (cx === 0) ctx.moveTo(pad.l + cx, py);
      else ctx.lineTo(pad.l + cx, py);
    }
    ctx.stroke();

    // Draw points: seeds blue, samples red
    for (var p = 0; p < pts.length; p++) {
      var pt = pts[p];
      var px = pad.l + (pt.lux / xMax) * pw;
      var pyPt = pad.t + ph - (pt.bri / yMax) * ph;
      ctx.beginPath();
      ctx.arc(px, pyPt, pt.seed ? 3 : 4, 0, 2 * Math.PI);
      ctx.fillStyle = pt.seed ? '#48f' : '#f44';
      ctx.fill();
    }
  }

  function fetchAndDraw() {
    fetch('/api/lux/points').then(function(r) { return r.json(); })
      .then(function(data) {
        drawChart(data);
        if (data.full && sampleBtn) sampleBtn.disabled = true;
      })
      .catch(function() {});
  }

  function init() {
    sampleBtn       = document.getElementById('luxcal-sample');
    fitBtn          = document.getElementById('luxcal-fit');
    clearBtn        = document.getElementById('luxcal-clear');
    downloadBtn     = document.getElementById('luxcal-download');
    acceptBtn       = document.getElementById('luxcal-accept');
    statusEl        = document.getElementById('luxcal-status');
    sampleCountEl   = document.getElementById('luxcal-count');
    luxValueEl      = document.getElementById('luxcal-lux');
    brightnessValueEl = document.getElementById('luxcal-bri');
    fitResultEl     = document.getElementById('luxcal-fitresult');
    newFitEl        = document.getElementById('luxcal-newfit');
    newParamsEl     = document.getElementById('luxcal-newparams');
    briSlider       = document.getElementById('luxcal-brightness');
    briLabel        = document.getElementById('luxcal-bri-num');
    chartCanvas     = document.getElementById('luxcal-chart');

    if (briSlider && briLabel) {
      briSlider.oninput = function() {
        briLabel.textContent = sliderToBrightness(parseInt(briSlider.value, 10)) + '%';
      };
      briSlider.onchange = function() {
        var bri = sliderToBrightness(parseInt(briSlider.value, 10));
        briLabel.textContent = bri + '%';
        fetch('/setBrightness?value=' + bri, { method: 'POST' }).catch(function() {});
      };
    }

    if (sampleBtn)   sampleBtn.onclick = takeSample;
    if (fitBtn)      fitBtn.onclick = doFit;
    if (clearBtn)    clearBtn.onclick = doClear;
    if (downloadBtn) downloadBtn.onclick = doDownload;
    if (acceptBtn)   acceptBtn.onclick = doAccept;

    // Disable sample button initially (mode off until modal opens)
    if (sampleBtn) sampleBtn.disabled = true;

    // SSE: update UI when firmware captures sample asynchronously
    Kwal.sse.onLuxcal(function(data) {
      showCount(data.realCount || 0);
      if (luxValueEl) luxValueEl.textContent = data.lux.toFixed(1);
      if (brightnessValueEl) brightnessValueEl.textContent = data.brightness.toFixed(1);
      fetchAndDraw();
    });

    // SSE: auto-fit triggered by firmware — show accept UI
    Kwal.sse.onLuxcalFit(function(data) {
      if (fitResultEl) {
        fitResultEl.textContent = 'Oud: ' + formatParams({
          brMax: data.oldBrMax, luxRate: data.oldLuxRate, luxMax: data.luxMax
        });
      }
      if (newParamsEl) {
        newParamsEl.textContent = 'Nieuw: ' + formatParams(data) +
          ' R\u00b2=' + (data.r2 != null ? Number(data.r2).toFixed(3) : '?') + ' (n=' + (data.realCount || 0) + ')';
      }
      if (newFitEl) newFitEl.style.display = 'block';
      lastFitParams = { brMax: data.brMax, luxRate: data.luxRate };
      if (sampleBtn) sampleBtn.disabled = true;
      fetchAndDraw();
      setStatus('Auto-fit — accepteer of wis');
    });
    // Early check: disable sun button if no lux sensor
    fetch('/api/lux/status').then(function(r) { return r.json(); })
      .then(function(data) {
        if (!data.hasLuxSensor) {
          var openBtn = document.getElementById('luxcal-open');
          if (openBtn) { openBtn.disabled = true; openBtn.style.opacity = '0.3'; }
        }
      }).catch(function() {});
  }

  return { init: init, loadStatus: loadStatus, onModalClose: onModalClose };
})();
