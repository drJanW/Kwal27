/**
 * @file    luxcal.js
 * @version 260319A
 * @date    2026-03-19
 *
 * Kwal - Lux Calibration module
 * Calibration panel for lux → brightness curve fitting
 * API: POST /api/lux/calibrate, /api/lux/sample, /api/lux/status,
 *       /api/lux/fit, /api/lux/clear, /api/lux/reload, GET /api/lux/csv
 */
Kwal.luxcal = (function() {
  'use strict';

  var sampleBtn, approveBtn, clearBtn, resetBtn, downloadBtn, acceptBtn;
  var statusEl, luxValueEl, brightnessValueEl, fitResultEl;
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
           ' slope=' + (d.luxRate != null ? Number(d.luxRate).toFixed(4) : '?');
    if (d.luxMax != null) s += ' (lux 0-' + Number(d.luxMax).toFixed(0) + ')';
    return s;
  }

  function setStatus(text) {
    if (statusEl) statusEl.textContent = text;
  }

  function hideAccept() {
    if (newFitEl) newFitEl.style.display = 'none';
  }

  var lastFitParams = null;

  function setDisabledAll(disabled) {
    if (sampleBtn)   sampleBtn.disabled = disabled;
    if (briSlider)   briSlider.disabled = disabled;
  }

  function updateUI(data) {
    if (!data) return;
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
          if (luxValueEl) luxValueEl.textContent = data.lux.toFixed(1);
          if (brightnessValueEl) brightnessValueEl.textContent = data.brightness.toFixed(1);
        } else {
          setStatus(data.error || 'Fout');
        }
        sampleBtn.disabled = false;
      })
      .catch(function() { setStatus('Sample mislukt'); sampleBtn.disabled = false; });
  }



  function doAccept() {
    setStatus('Opslaan...');
    fetch('/api/lux/accept', { method: 'POST' })
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (data.ok) {
          hideAccept();
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

  function doApprove() {
    setStatus('Fitting + opslaan...');
    fetch('/api/lux/fit', { method: 'POST' })
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (!data.ok) throw new Error(data.error || 'Fit mislukt');
        return fetch('/api/lux/accept', { method: 'POST' })
          .then(function(r) { return r.json(); });
      })
      .then(function(data) {
        if (data.ok) {
          hideAccept();
          if (sampleBtn) sampleBtn.disabled = false;
          fetchAndDraw();
          setStatus('Opgeslagen, seeds vernieuwd');
        } else {
          setStatus('Opslaan mislukt');
        }
      })
      .catch(function(e) { setStatus(e.message || 'Approve mislukt'); });
  }

  function doClear() {
    if (!confirm('Wis samples?')) return;
    fetch('/api/lux/clear', { method: 'POST' })
      .then(function(r) { return r.json(); })
      .then(function(data) {
        hideAccept();
        if (sampleBtn) sampleBtn.disabled = false;
        fetchAndDraw();
        setStatus('Samples gewist');
      })
      .catch(function() { setStatus('Wissen mislukt'); });
  }

  function doReset() {
    if (!confirm('Reset naar fabrieksinstellingen?')) return;
    fetch('/api/lux/reset', { method: 'POST' })
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (fitResultEl) fitResultEl.textContent = '-';
        hideAccept();
        if (sampleBtn) sampleBtn.disabled = false;
        fetchAndDraw();
        setStatus('Reset naar defaults');
      })
      .catch(function() { setStatus('Reset mislukt'); });
  }

  function doDownload() {
    window.open('/api/lux/csv', '_blank');
  }

  // JS-side Gauss-Newton fit: y = B * (1 - exp(-r * lux))
  function jsFit(pts, initB, initR) {
    if (!pts || pts.length < 2) return null;
    var B = initB || 100, r = initR || 0.02;
    for (var iter = 0; iter < 12; iter++) {
      var jj00 = 0, jj01 = 0, jj11 = 0, jr0 = 0, jr1 = 0;
      for (var i = 0; i < pts.length; i++) {
        var lux = Math.max(pts[i].lux, 0);
        var obs = pts[i].bri;
        var e = 1 - Math.exp(-r * lux);
        var res = obs - B * e;
        var w = 1 / (1 + lux);
        var j0 = e, j1 = B * lux * (1 - e);
        jj00 += w * j0 * j0; jj01 += w * j0 * j1;
        jj11 += w * j1 * j1; jr0 += w * j0 * res; jr1 += w * j1 * res;
      }
      var det = jj00 * jj11 - jj01 * jj01;
      if (Math.abs(det) < 1e-20) break;
      var dB = (jj11 * jr0 - jj01 * jr1) / det;
      var dr = (-jj01 * jr0 + jj00 * jr1) / det;
      B += dB; r += dr;
      if (B < 10) B = 10; if (B > 500) B = 500;
      if (r < 0.001) r = 0.001; if (r > 0.5) r = 0.5;
      if (Math.abs(dB) < 1e-6 && Math.abs(dr) < 1e-6) break;
    }
    var ssRes = 0, ssTot = 0, meanY = 0;
    for (var j = 0; j < pts.length; j++) meanY += pts[j].bri;
    meanY /= pts.length;
    for (var k = 0; k < pts.length; k++) {
      var lk = Math.max(pts[k].lux, 0);
      var diff = pts[k].bri - B * (1 - Math.exp(-r * lk));
      ssRes += diff * diff;
      var dm = pts[k].bri - meanY;
      ssTot += dm * dm;
    }
    return { brMax: B, luxRate: r, r2: ssTot > 0 ? 1 - ssRes / ssTot : 0 };
  }

  function drawChart(data) {
    if (!chartCanvas) return;
    var ctx = chartCanvas.getContext('2d');
    var W = chartCanvas.width, H = chartCanvas.height;
    var pad = { l: 36, r: 10, t: 10, b: 32 };
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

    // Quadratic X-axis: pixel = sqrt(lux/xMax) * pw, lux = (px/pw)² * xMax
    function luxToX(lux) { return Math.sqrt(lux / xMax) * pw; }
    function xToLux(px)  { var t = px / pw; return t * t * xMax; }

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
      var xv = (k / xSteps) * (k / xSteps) * xMax;
      var xp = pad.l + (k / xSteps) * pw;
      ctx.beginPath(); ctx.moveTo(xp, pad.t); ctx.lineTo(xp, pad.t + ph); ctx.stroke();
      ctx.fillText(Math.round(xv), xp, pad.t + ph + 4);
    }

    // Axis labels
    ctx.fillStyle = '#888';
    ctx.textAlign = 'center';
    ctx.fillText('lux', pad.l + pw / 2, pad.t + ph + 20);
    ctx.save();
    ctx.translate(8, pad.t + ph / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText('bri', 0, 0);
    ctx.restore();

    // Seeds-only fit (yellow) — the curve the seeds represent
    var seedBrMax = data.seedBrMax;
    var seedLuxRate = data.seedLuxRate;
    if (seedBrMax != null && seedLuxRate != null) {
      ctx.strokeStyle = '#fc0';
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      for (var cx = 0; cx <= pw; cx++) {
        var lux = xToLux(cx);
        var bri = seedBrMax * (1.0 - Math.exp(-seedLuxRate * lux));
        var py = pad.t + ph - (bri / yMax) * ph;
        if (cx === 0) ctx.moveTo(pad.l + cx, py);
        else ctx.lineTo(pad.l + cx, py);
      }
      ctx.stroke();
    }

    // Live fit curve (green)
    if (data.fitBrMax != null && data.fitLuxRate != null) {
      ctx.strokeStyle = '#0c0';
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      for (var cx2 = 0; cx2 <= pw; cx2++) {
        var lux2 = xToLux(cx2);
        var bri2 = data.fitBrMax * (1.0 - Math.exp(-data.fitLuxRate * lux2));
        var py2 = pad.t + ph - (bri2 / yMax) * ph;
        if (cx2 === 0) ctx.moveTo(pad.l + cx2, py2);
        else ctx.lineTo(pad.l + cx2, py2);
      }
      ctx.stroke();
    }

    // Draw points: seeds blue, samples red
    for (var p = 0; p < pts.length; p++) {
      var pt = pts[p];
      var px = pad.l + luxToX(pt.lux);
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
        // Compute fits from data: seeds-only + all-points
        var pts = data.points || [];
        var seedCount = data.seedCount || 0;
        var seeds = pts.slice(0, seedCount);
        var sc = pts.length - seedCount;
        // Seeds-only fit → "old" row
        var seedFit = jsFit(seeds, data.brMax, data.luxRate);
        if (seedFit) {
          data.seedBrMax = seedFit.brMax;
          data.seedLuxRate = seedFit.luxRate;
          data.seedR2 = seedFit.r2;
        }
        // All-points fit → "new" row
        if (data.fitBrMax == null && pts.length >= 2) {
          var allFit = jsFit(pts, data.brMax, data.luxRate);
          if (allFit) { data.fitBrMax = allFit.brMax; data.fitLuxRate = allFit.luxRate; data.fitR2 = allFit.r2; }
        }
        drawChart(data);
        if (data.full && sampleBtn) sampleBtn.disabled = true;
        // Always show table: old + new(+N)
        if (fitResultEl) {
          var lm = data.luxMax ? Number(data.luxMax).toFixed(0) : '?';
          var ts = 'width:100%;font-size:0.75rem;color:#aaa;border-collapse:collapse';
          var html = '<table style="' + ts + '">';
          html += '<tr><th style="text-align:left;width:35%">0-' + lm + '</th>';
          html += '<th style="text-align:right;width:25%">brMax</th>';
          html += '<th style="text-align:right;width:22%">Slope</th>';
          html += '<th style="text-align:right;width:18%">R\u00b2</th></tr>';
          html += '<tr style="color:#fc0"><td>now</td>';
          html += '<td style="text-align:right">' + (data.seedBrMax != null ? Number(data.seedBrMax).toFixed(1) : '?') + '</td>';
          html += '<td style="text-align:right">' + (data.seedLuxRate != null ? Number(data.seedLuxRate).toFixed(4) : '?') + '</td>';
          html += '<td style="text-align:right"></td></tr>';
          if (data.fitBrMax != null) {
            html += '<tr style="color:#0c0"><td>new(+' + sc + ')</td>';
            html += '<td style="text-align:right">' + Number(data.fitBrMax).toFixed(1) + '</td>';
            html += '<td style="text-align:right">' + Number(data.fitLuxRate).toFixed(4) + '</td>';
            html += '<td style="text-align:right">' + Number(data.fitR2).toFixed(3) + '</td></tr>';
          }
          html += '</table>';
          fitResultEl.innerHTML = html;
        }
      })
      .catch(function() {});
  }

  function init() {
    sampleBtn       = document.getElementById('luxcal-sample');
    approveBtn      = document.getElementById('luxcal-approve');
    clearBtn        = document.getElementById('luxcal-clear');
    resetBtn        = document.getElementById('luxcal-reset');
    downloadBtn     = document.getElementById('luxcal-download');
    acceptBtn       = document.getElementById('luxcal-accept');
    statusEl        = document.getElementById('luxcal-status');
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
    if (approveBtn)  approveBtn.onclick = doApprove;
    if (clearBtn)    clearBtn.onclick = doClear;
    if (resetBtn)    resetBtn.onclick = doReset;
    if (downloadBtn) downloadBtn.onclick = doDownload;
    if (acceptBtn)   acceptBtn.onclick = doAccept;

    // Disable sample button initially (mode off until modal opens)
    if (sampleBtn) sampleBtn.disabled = true;

    // SSE: update UI when firmware captures sample asynchronously
    Kwal.sse.onLuxcal(function(data) {
      if (luxValueEl) luxValueEl.textContent = data.lux.toFixed(1);
      if (brightnessValueEl) brightnessValueEl.textContent = data.brightness.toFixed(1);
      fetchAndDraw();
    });

    // SSE: auto-fit triggered by firmware — show accept UI
    Kwal.sse.onLuxcalFit(function(data) {
      lastFitParams = { brMax: data.brMax, luxRate: data.luxRate };
      if (sampleBtn) sampleBtn.disabled = true;
      fetchAndDraw();
      setStatus('Auto-fit \u2014 klik \u2713 om te accepteren');
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
