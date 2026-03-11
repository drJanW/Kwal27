/*
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
  var savedBrightness = -1;  // brightness before modal open (-1 = not saved)
  var hasLuxSensor = true;   // assume present until status says otherwise

  // Quadratic slider mapping: more resolution at low brightness
  function sliderToBrightness(v) { return Math.round(v * v / 100); }
  function brightnessToSlider(b) { return Math.round(Math.sqrt(b * 100)); }

  function formatParams(d) {
    return 'max=' + d.luxMax + ' lo=' + d.luxShiftLo + ' hi=' + d.luxShiftHi + ' \u03b3=' + d.luxGamma;
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
        if (fitResultEl && typeof data.luxMax === 'number') {
          fitResultEl.textContent = 'Huidig: ' + formatParams(data);
        }
        hideAccept();
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
              luxMax: data.oldLuxMax, luxShiftLo: data.oldLuxShiftLo,
              luxShiftHi: data.oldLuxShiftHi, luxGamma: data.oldLuxGamma
            });
          }
          if (newParamsEl) {
            newParamsEl.textContent = 'Nieuw: ' + formatParams(data) +
              ' err=' + data.error + ' (n=' + (data.realCount || 0) + ')';
          }
          if (newFitEl) newFitEl.style.display = 'block';          lastFitParams = { luxMax: data.luxMax, luxShiftLo: data.luxShiftLo,
            luxShiftHi: data.luxShiftHi, luxGamma: data.luxGamma };          setStatus('Fit berekend — accepteer of sample verder');
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
        setStatus('Reset naar seeds');
      })
      .catch(function() { setStatus('Reset mislukt'); });
  }

  function doDownload() {
    window.open('/api/lux/csv', '_blank');
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
    });

    // SSE: auto-fit triggered by firmware — show accept UI
    Kwal.sse.onLuxcalFit(function(data) {
      if (fitResultEl) {
        fitResultEl.textContent = 'Oud: ' + formatParams({
          luxMax: data.oldLuxMax, luxShiftLo: data.oldLuxShiftLo,
          luxShiftHi: data.oldLuxShiftHi, luxGamma: data.oldLuxGamma
        });
      }
      if (newParamsEl) {
        newParamsEl.textContent = 'Nieuw: ' + formatParams(data) +
          ' err=' + data.error + ' (n=' + (data.realCount || 0) + ')';
      }
      if (newFitEl) newFitEl.style.display = 'block';
      lastFitParams = { luxMax: data.luxMax, luxShiftLo: data.luxShiftLo,
        luxShiftHi: data.luxShiftHi, luxGamma: data.luxGamma };
      setStatus('Auto-fit — accepteer of sample verder');
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
