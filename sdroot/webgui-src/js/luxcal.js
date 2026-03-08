/*
 * Kwal - Lux Calibration module
 * Calibration panel for lux → brightness curve fitting
 * API: POST /api/lux/calibrate, /api/lux/sample, /api/lux/status,
 *       /api/lux/fit, /api/lux/clear, /api/lux/reload, GET /api/lux/csv
 */
Kwal.luxcal = (function() {
  'use strict';

  var sampleBtn, fitBtn, clearBtn, downloadBtn;
  var statusEl, sampleCountEl, luxValueEl, brightnessValueEl, fitResultEl;
  var briSlider, briLabel;
  var savedBrightness = -1;  // brightness before modal open (-1 = not saved)
  var hasLuxSensor = true;   // assume present until status says otherwise

  function formatParams(prefix, d) {
    return prefix + 'max=' + d.luxMax + ' lo=' + d.luxShiftLo + ' hi=' + d.luxShiftHi + ' γ=' + d.luxGamma;
  }

  function setStatus(text) {
    if (statusEl) statusEl.textContent = text;
  }

  function setDisabledAll(disabled) {
    if (sampleBtn)   sampleBtn.disabled = disabled;
    if (fitBtn)      fitBtn.disabled = disabled;
    if (clearBtn)    clearBtn.disabled = disabled;
    if (downloadBtn) downloadBtn.disabled = disabled;
    if (briSlider)   briSlider.disabled = disabled;
  }

  function updateUI(data) {
    if (!data) return;
    if (sampleCountEl) sampleCountEl.textContent = (data.sampleCount || 0) + ' data points';
    if (typeof data.lastLux === 'number' && luxValueEl) {
      luxValueEl.textContent = data.lastLux.toFixed(1) + ' lux';
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
      briSlider.value = savedBrightness;
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
        if (fitResultEl && typeof data.luxMax === 'number') {
          fitResultEl.textContent = formatParams('Huidig: ', data);
        }
        // Auto-enable calibration mode on modal open
        fetch('/api/lux/calibrate?mode=on', { method: 'POST' })
          .then(function(r) { return r.json(); })
          .then(function(d) {
            updateUI(d);
            if (sampleBtn) sampleBtn.disabled = false;
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
      if (briSlider) briSlider.value = savedBrightness;
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
          if (sampleCountEl) sampleCountEl.textContent = (data.n || 0) + ' data points';
          if (luxValueEl) luxValueEl.textContent = data.lux.toFixed(1) + ' lux';
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
          var txt = formatParams('Oud: ', {
            luxMax: data.oldLuxMax, luxShiftLo: data.oldLuxShiftLo,
            luxShiftHi: data.oldLuxShiftHi, luxGamma: data.oldLuxGamma
          });
          txt += '\nNieuw: max=' + data.luxMax +
            ' lo=' + data.luxShiftLo +
            ' hi=' + data.luxShiftHi +
            ' γ=' + data.luxGamma +
            ' err=' + data.error +
            ' (n=' + data.sampleCount + ')';
          if (fitResultEl) fitResultEl.textContent = txt;
          var nc = typeof data.newSampleCount === 'number' ? data.newSampleCount : 0;
          if (sampleCountEl) sampleCountEl.textContent = nc + ' data points';
          setStatus('Fit opgeslagen, seeds vernieuwd');
        } else {
          setStatus(data.error || 'Fit mislukt');
        }
      })
      .catch(function() { setStatus('Fit mislukt'); });
  }

  function doClear() {
    if (!confirm('Alle data wissen?')) return;
    fetch('/api/lux/clear', { method: 'POST' })
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (sampleCountEl) sampleCountEl.textContent = '0 data points';
        if (fitResultEl) fitResultEl.textContent = '-';
        setStatus('Gewist');
      })
      .catch(function() { setStatus('Wissen mislukt'); });
  }

  function doDownload() {
    window.open('/api/lux/csv', '_blank');
  }

  function init() {
    sampleBtn       = document.getElementById('luxcal-sample');
    fitBtn          = document.getElementById('luxcal-fit');
    clearBtn        = document.getElementById('luxcal-clear');
    downloadBtn     = document.getElementById('luxcal-download');
    statusEl        = document.getElementById('luxcal-status');
    sampleCountEl   = document.getElementById('luxcal-count');
    luxValueEl      = document.getElementById('luxcal-lux');
    brightnessValueEl = document.getElementById('luxcal-bri');
    fitResultEl     = document.getElementById('luxcal-fitresult');
    briSlider       = document.getElementById('luxcal-brightness');
    briLabel        = document.getElementById('luxcal-bri-num');

    if (briSlider && briLabel) {
      briSlider.oninput = function() {
        briLabel.textContent = briSlider.value + '%';
      };
      briSlider.onchange = function() {
        var v = parseInt(briSlider.value, 10);
        briLabel.textContent = v + '%';
        fetch('/setBrightness?value=' + v, { method: 'POST' }).catch(function() {});
      };
    }

    if (sampleBtn)   sampleBtn.onclick = takeSample;
    if (fitBtn)      fitBtn.onclick = doFit;
    if (clearBtn)    clearBtn.onclick = doClear;
    if (downloadBtn) downloadBtn.onclick = doDownload;

    // Disable sample button initially (mode off until modal opens)
    if (sampleBtn) sampleBtn.disabled = true;

    // SSE: update UI when firmware captures sample asynchronously
    Kwal.sse.onLuxcal(function(data) {
      if (sampleCountEl) sampleCountEl.textContent = (data.n || 0) + ' data points';
      if (luxValueEl) luxValueEl.textContent = data.lux.toFixed(1) + ' lux';
      if (brightnessValueEl) brightnessValueEl.textContent = data.brightness.toFixed(1);
    });
  }

  return { init: init, loadStatus: loadStatus, onModalClose: onModalClose };
})();
