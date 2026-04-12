/**
 * @file    adminSettings.js
 * @version 260412A
 * @date    2026-04-12
 *
 * Kwal WebGUI - Admin settings modal (globals.csv editor)
 * PIN-gated, full CRUD with Save & Restart / Cancel & Restart.
 */
Kwal.adminSettings = (function() {
  'use strict';

  var pin = '';
  var originalData = null;  // snapshot from GET for dirty detection

  // ── Validation rules (hardcoded) ───────────────────────────────
  // Lo/Hi pairs: Lo key must be <= Hi key
  var loHiPairs = [
    ['brightnessLo', 'brightnessHi'],
    ['volumeLo', 'volumeHi'],  // note: volumeHi not in CSV but kept for safety
    ['heartbeatMinMs', 'heartbeatMaxMs'],
    ['distanceMinMm', 'distanceMaxMm'],
    ['minAudioIntervalMs', 'maxAudioIntervalMs'],
    ['singleDirMinIntervalMs', 'singleDirMaxIntervalMs'],
    ['minSaytimeIntervalMs', 'maxSaytimeIntervalMs'],
    ['minTemperatureSpeakIntervalMs', 'maxTemperatureSpeakIntervalMs'],
    ['tvAudioMinMs', 'tvAudioMaxMs'],
    ['calendarShiftLo', 'calendarShiftHi'],
    ['pingVolumeMin', 'pingVolumeMax']
  ];

  // Fraction keys: value must be 0.0–1.0
  var fractionKeys = [
    'volumeLo', 'basePlaybackVolume', 'minDistanceVolume',
    'pingVolumeMax', 'pingVolumeMin'
  ];

  // Keys that must be > 0 (intervals, durations)
  var positiveKeys = [
    'minAudioIntervalMs', 'maxAudioIntervalMs', 'singleDirMinIntervalMs',
    'singleDirMaxIntervalMs', 'baseFadeMs', 'webAudioNextFadeMs', 'busyRetryMs',
    'minSaytimeIntervalMs', 'maxSaytimeIntervalMs',
    'minTemperatureSpeakIntervalMs', 'maxTemperatureSpeakIntervalMs',
    'lightFallbackIntervalMs', 'shiftCheckIntervalMs',
    'colorChangeIntervalMs', 'patternChangeIntervalMs',
    'luxMeasurementDelayMs', 'luxMeasurementIntervalMs',
    'distanceSensorInitDelayMs', 'luxSensorInitDelayMs',
    'sensorBaseDefaultMs', 'sensorFastIntervalMs', 'sensorFastDurationMs',
    'distanceNewWindowMs', 'heartbeatMinMs', 'heartbeatMaxMs', 'heartbeatDefaultMs',
    'flashBurstIntervalMs', 'reminderIntervalMs', 'flashCriticalMs', 'flashNormalMs',
    'clockBootstrapIntervalMs', 'ntpFallbackTimeoutMs', 'bootPhaseMs',
    'rtcTemperatureIntervalMs', 'weatherRefreshIntervalMs', 'sunRefreshIntervalMs',
    'calendarRefreshIntervalMs', 'csvHttpTimeoutMs', 'csvFetchWaitMs',
    'sdHealthCheckIntervalMs', 'tvAudioMinMs', 'tvAudioMaxMs',
    'timerStatusIntervalMs'
  ];

  // ── DOM refs ───────────────────────────────────────────────────
  var modal, pinInput, pinBtn, content, pinGate, sectionsDiv;
  var saveBtn, cancelBtn;

  function init() {
    modal      = document.getElementById('admin-settings-modal');
    pinInput   = document.getElementById('admin-pin');
    pinBtn     = document.getElementById('admin-pin-btn');
    pinGate    = document.getElementById('admin-pin-gate');
    content    = document.getElementById('admin-content');
    sectionsDiv = document.getElementById('admin-sections');
    saveBtn    = document.getElementById('admin-save-btn');
    cancelBtn  = document.getElementById('admin-cancel-btn');

    if (pinBtn)    pinBtn.onclick = unlock;
    if (pinInput)  pinInput.onkeydown = function(e) { if (e.key === 'Enter') unlock(); };
    if (saveBtn)   saveBtn.onclick = save;
    if (cancelBtn) cancelBtn.onclick = cancelRestart;
  }

  function show() {
    // Reset to PIN gate on each open
    pin = '';
    if (pinInput)  pinInput.value = '';
    if (pinGate)   pinGate.style.display = '';
    if (content)   content.style.display = 'none';
    if (sectionsDiv) sectionsDiv.innerHTML = '';
    originalData = null;
    Kwal.modal.lockBackdrop();
  }

  function unlock() {
    if (!pinInput) return;
    pin = pinInput.value.trim();
    if (!pin) return;
    
    fetch('/api/admin/globals?pin=' + encodeURIComponent(pin))
      .then(function(r) {
        if (r.status === 403) {
          // Silent reject — clear input
          pinInput.value = '';
          pin = '';
          return null;
        }
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.json();
      })
      .then(function(data) {
        if (!data) return;
        originalData = JSON.parse(JSON.stringify(data));
        if (pinGate) pinGate.style.display = 'none';
        if (content) content.style.display = '';
        render(data);
      })
      .catch(function(err) {
        console.error('Admin load failed:', err);
      });
  }

  function render(data) {
    if (!sectionsDiv) return;
    sectionsDiv.innerHTML = '';

    var currentSection = null;
    var sectionEl = null;

    for (var i = 0; i < data.length; i++) {
      var item = data[i];

      if (item.section) {
        // Section header — collapsible group
        var group = document.createElement('div');
        group.className = 'admin-section';

        var header = document.createElement('h4');
        header.textContent = item.section;
        header.className = 'admin-section-header';
        header.onclick = (function(g) {
          return function() { g.classList.toggle('collapsed'); };
        })(group);
        group.appendChild(header);

        sectionEl = document.createElement('div');
        sectionEl.className = 'admin-section-body';
        group.appendChild(sectionEl);
        sectionsDiv.appendChild(group);
        currentSection = item.section;
        continue;
      }

      if (!item.key) continue;

      var target = sectionEl || sectionsDiv;
      var row = document.createElement('div');
      row.className = 'edit-row' + (item.active === false ? ' disabled' : '');
      row.setAttribute('data-key', item.key);

      // Checkbox for active/commented
      var cb = document.createElement('input');
      cb.type = 'checkbox';
      cb.className = 'admin-active';
      cb.checked = item.active !== false;
      cb.setAttribute('data-key', item.key);
      cb.onchange = (function(r, inp) {
        return function() {
          if (this.checked) {
            r.classList.remove('disabled');
            inp.disabled = false;
          } else {
            r.classList.add('disabled');
            inp.disabled = true;
          }
          markRowDirty(r, item.key);
        };
      })(row, null);  // inp set below

      // Label (key name) with comment as tooltip
      var label = document.createElement('label');
      label.textContent = item.key;
      if (item.comment) label.title = item.comment;

      // Value input
      var inp = document.createElement('input');
      inp.type = 'text';
      inp.value = item.value || '';
      inp.setAttribute('data-key', item.key);
      inp.setAttribute('data-type', item.type);
      inp.setAttribute('data-orig', item.value || '');
      inp.disabled = item.active === false;
      inp.oninput = (function(r, c, k) {
        return function() {
          // Auto-uncomment: if user edits a disabled field, auto-check
          if (!c.checked) {
            c.checked = true;
            r.classList.remove('disabled');
            this.disabled = false;
          }
          markRowDirty(r, k);
          clearRowError(r);
        };
      })(row, cb, item.key);

      // Fix the checkbox closure reference
      cb.onchange = (function(r, input, k) {
        return function() {
          if (this.checked) {
            r.classList.remove('disabled');
            input.disabled = false;
          } else {
            r.classList.add('disabled');
            input.disabled = true;
          }
          markRowDirty(r, k);
        };
      })(row, inp, item.key);

      row.appendChild(cb);
      row.appendChild(label);
      row.appendChild(inp);
      target.appendChild(row);
    }
  }

  function markRowDirty(row, key) {
    // Compare current value+active state with original
    var inp = row.querySelector('input[type="text"]');
    var cb = row.querySelector('input[type="checkbox"]');
    if (!inp || !cb) return;

    var origItem = findOriginal(key);
    var changed = false;
    if (origItem) {
      if (inp.value !== (origItem.value || '')) changed = true;
      if (cb.checked !== (origItem.active !== false)) changed = true;
    }
    if (changed) {
      row.classList.add('dirty');
    } else {
      row.classList.remove('dirty');
    }
  }

  function findOriginal(key) {
    if (!originalData) return null;
    for (var i = 0; i < originalData.length; i++) {
      if (originalData[i].key === key) return originalData[i];
    }
    return null;
  }

  function clearRowError(row) {
    row.classList.remove('validation-error');
    var inp = row.querySelector('input[type="text"]');
    if (inp) inp.title = '';
  }

  // ── Validation ─────────────────────────────────────────────────
  function validate() {
    var rows = sectionsDiv.querySelectorAll('.edit-row');
    var errors = [];
    var values = {};  // key → {value, active, type} for cross-field checks

    // Collect all values first
    for (var i = 0; i < rows.length; i++) {
      var row = rows[i];
      var inp = row.querySelector('input[type="text"]');
      var cb = row.querySelector('input[type="checkbox"]');
      if (!inp || !cb) continue;
      var key = inp.getAttribute('data-key');
      var type = inp.getAttribute('data-type');
      values[key] = { value: inp.value.trim(), active: cb.checked, type: type, row: row };
    }

    // Per-field type validation (only active fields)
    for (var key in values) {
      var v = values[key];
      if (!v.active) continue;
      var val = v.value;
      var err = null;

      if (v.type === 'u') {
        if (!/^\d+$/.test(val)) err = 'Must be a non-negative integer';
      } else if (v.type === 'f') {
        if (isNaN(parseFloat(val)) || !isFinite(val)) err = 'Must be a valid number';
      } else if (v.type === 'i') {
        if (!/^-?\d+$/.test(val)) err = 'Must be an integer';
      } else if (v.type === 'b') {
        if (val !== '0' && val !== '1') err = 'Must be 0 or 1';
      } else if (v.type === 's') {
        if (!val) err = 'Must not be empty';
      }

      if (err) {
        errors.push(key + ': ' + err);
        v.row.classList.add('validation-error');
        var inp2 = v.row.querySelector('input[type="text"]');
        if (inp2) inp2.title = err;
      }
    }

    // Fraction range check
    for (var fi = 0; fi < fractionKeys.length; fi++) {
      var fk = fractionKeys[fi];
      if (values[fk] && values[fk].active) {
        var fv = parseFloat(values[fk].value);
        if (!isNaN(fv) && (fv < 0.0 || fv > 1.0)) {
          errors.push(fk + ': must be 0.0–1.0');
          values[fk].row.classList.add('validation-error');
        }
      }
    }

    // Positive value check
    for (var pi = 0; pi < positiveKeys.length; pi++) {
      var pk = positiveKeys[pi];
      if (values[pk] && values[pk].active) {
        var pval = (values[pk].type === 'f') ? parseFloat(values[pk].value) : parseInt(values[pk].value, 10);
        if (!isNaN(pval) && pval <= 0) {
          errors.push(pk + ': must be > 0');
          values[pk].row.classList.add('validation-error');
        }
      }
    }

    // Lo/Hi pair checks
    for (var li = 0; li < loHiPairs.length; li++) {
      var loKey = loHiPairs[li][0];
      var hiKey = loHiPairs[li][1];
      if (values[loKey] && values[hiKey] && values[loKey].active && values[hiKey].active) {
        var loVal = parseFloat(values[loKey].value);
        var hiVal = parseFloat(values[hiKey].value);
        if (!isNaN(loVal) && !isNaN(hiVal) && loVal > hiVal) {
          errors.push(loKey + ' (' + loVal + ') must be <= ' + hiKey + ' (' + hiVal + ')');
          values[loKey].row.classList.add('validation-error');
          values[hiKey].row.classList.add('validation-error');
        }
      }
    }

    return errors;
  }

  // ── Collect data for POST ──────────────────────────────────────
  function collectData() {
    // Rebuild the full array: sections + entries in original order
    if (!originalData) return [];

    var result = [];
    var sectionEntries = {};  // key → current row data

    // Gather current row state
    var rows = sectionsDiv.querySelectorAll('.edit-row');
    for (var i = 0; i < rows.length; i++) {
      var row = rows[i];
      var inp = row.querySelector('input[type="text"]');
      var cb = row.querySelector('input[type="checkbox"]');
      if (!inp || !cb) continue;
      var key = inp.getAttribute('data-key');
      sectionEntries[key] = {
        value: inp.value.trim(),
        active: cb.checked,
        type: inp.getAttribute('data-type')
      };
    }

    // Rebuild in original order with updated values
    for (var j = 0; j < originalData.length; j++) {
      var orig = originalData[j];
      if (orig.section) {
        result.push({ section: orig.section });
        continue;
      }
      if (!orig.key) continue;

      var current = sectionEntries[orig.key];
      if (current) {
        result.push({
          key: orig.key,
          type: current.type || orig.type,
          value: current.value,
          comment: orig.comment || '',
          active: current.active
        });
      } else {
        // Key not in DOM (shouldn't happen), keep original
        result.push(orig);
      }
    }

    return result;
  }

  // ── Save & Restart ─────────────────────────────────────────────
  function save() {
    // Clear previous errors
    var errorRows = sectionsDiv.querySelectorAll('.validation-error');
    for (var i = 0; i < errorRows.length; i++) {
      errorRows[i].classList.remove('validation-error');
    }

    var errors = validate();
    if (errors.length > 0) {
      alert('Validation errors:\n\n' + errors.join('\n'));
      // Scroll to first error
      var firstErr = sectionsDiv.querySelector('.validation-error');
      if (firstErr) firstErr.scrollIntoView({ behavior: 'smooth', block: 'center' });
      return;
    }

    if (!confirm('Save settings and restart ESP?')) return;

    var data = collectData();
    fetch('/api/admin/globals?pin=' + encodeURIComponent(pin), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data)
    })
    .then(function(r) {
      if (!r.ok) return r.json().then(function(j) { throw new Error(j.error || 'Save failed'); });
      return r.json();
    })
    .then(function() {
      // Save succeeded — restart
      return fetch('/api/restart', { method: 'POST' });
    })
    .then(function() {
      document.body.innerHTML = '<div style="text-align:center;margin-top:30vh;color:#ccc;font-size:1.5rem">Settings saved. Restarting...</div>';
    })
    .catch(function(err) {
      alert('Save failed: ' + err.message);
    });
  }

  // ── Cancel & Restart ───────────────────────────────────────────
  function cancelRestart() {
    if (!confirm('Discard changes and restart ESP?')) return;

    fetch('/api/restart', { method: 'POST' })
      .then(function() {
        document.body.innerHTML = '<div style="text-align:center;margin-top:30vh;color:#ccc;font-size:1.5rem">Restarting...</div>';
      })
      .catch(function(err) {
        alert('Restart failed: ' + err.message);
      });
  }

  return { init: init, show: show };
})();
