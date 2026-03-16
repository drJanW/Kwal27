/**
 * @file    health.js
 * @version 260316C
 * @date    2026-03-16
 *
 * Kwal - Health module
 * System health status display in DEV modal
 * API: GET /api/health, POST /api/restart, POST /api/sleep
 */
Kwal.health = (function() {
  'use strict';

  // Health bit definitions (must match AlertState::getHealthBits())
  // KRITIEK: Volgorde MOET matchen met enum StatusComponent in AlertState.h!
  var FLAGS = [
    { bit: 0, name: 'SD',       icon: '💾' },
    { bit: 1, name: 'WiFi',     icon: '📶' },
    { bit: 2, name: 'RTC',      icon: '🕐' },
    { bit: 3, name: 'Audio',    icon: '🔊' },
    { bit: 4, name: 'Distance', icon: '📏' },
    { bit: 5, name: 'Lux',      icon: '☀️' },
    { bit: 6, name: 'Sensor3',  icon: '🌡️' },
    { bit: 7, name: 'NTP',      icon: '⏰' },
    { bit: 8, name: 'Weather',  icon: '🌤️' },
    { bit: 9, name: 'Calendar', icon: '📅' },
    { bit: 10, name: 'TTS',     icon: '🗣️' },
    { bit: 11, name: 'NAS',     icon: '🗄️' }
  ];

  // Status values for 4-bit fields (must match AlertState.h)
  var STATUS_OK = 0;
  var STATUS_NOTOK = 15;

  var container, sysRestartBtn, sysSleepBtn;

  // Extract 4-bit field from boot status uint64
  // Note: JS handles numbers up to 2^53 safely, uint64 fits
  function getStatusField(bootStatus, index) {
    return (bootStatus / Math.pow(16, index)) & 0xF | 0;
  }

  // Render status icon based on 4-bit value
  function renderStatus(value) {
    if (value === STATUS_OK) return '✅';
    if (value === STATUS_NOTOK) return '❌';
    return '⟳' + value;
  }

  function init() {
    container = document.getElementById('health-content');
    sysRestartBtn = document.getElementById('sys-restart-btn');
    sysSleepBtn = document.getElementById('sys-sleep-btn');
    if (sysRestartBtn) {
      sysRestartBtn.onclick = doRestart;
    }
    if (sysSleepBtn) {
      sysSleepBtn.onclick = doSleep;
    }
  }

  function load() {
    if (!container) return;
    container.innerHTML = '<em>Loading...</em>';

    fetch('/api/health')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        render(data);
      })
      .catch(function(err) {
        container.innerHTML = '<em>Error: ' + err.message + '</em>';
      });
  }

  function render(data) {
    if (!container) return;

    // Update title text (preserve refresh icon as first child)
    var titleEl = document.getElementById('health-title');
    if (titleEl) {
      var refreshEl = document.getElementById('health-refresh');
      var titleText = (data.device || 'Status') + (data.ntpDate ? ' ' + data.ntpDate : '');
      // Clear text nodes, keep refresh icon
      while (titleEl.lastChild && titleEl.lastChild !== refreshEl) {
        titleEl.removeChild(titleEl.lastChild);
      }
      titleEl.appendChild(document.createTextNode(titleText));
    }

    var html = '<table class="health-table">';

    // Flags with boot status (use boot field if available, fallback to health bits)
    var healthBits = data.health || 0;
    var bootStatus = data.boot || 0;
    var absentBits = data.absent || 0;
    var useBoot = (data.boot !== undefined);
    
    for (var i = 0; i < FLAGS.length; i++) {
      var f = FLAGS[i];
      var status;
      // Check if hardware is absent (not present per HWconfig)
      if (absentBits & (1 << f.bit)) {
        status = '—';
      } else if (useBoot) {
        var value = getStatusField(bootStatus, f.bit);
        status = renderStatus(value);
      } else {
        var ok = (healthBits & (1 << f.bit)) !== 0;
        status = ok ? '✅' : '❌';
      }
      // Append RTC temperature after RTC status
      if (f.name === 'RTC' && data.rtcTempC !== undefined) {
        status += ' ' + data.rtcTempC.toFixed(1) + '°';
      }
      // Append theme box name after Audio status
      if (f.name === 'Audio' && data.themeBox) {
        status += ' ' + data.themeBox;
      }
      // Append time after NTP status
      if (f.name === 'NTP' && data.ntpTime) {
        status += ' ' + data.ntpTime;
      }
      // Append calendar date after Calendar status
      if (f.name === 'Calendar' && data.calendarDate) {
        status += ' ' + data.calendarDate;
      }
      html += '<tr><td>' + f.icon + ' ' + f.name + '</td><td>' + status + '</td></tr>';
    }

    // Heap as component row
    if (data.heapFree !== undefined) {
      html += '<tr><td>🧠 Heap</td><td>' + data.heapFree + '>' + data.heapMin + 'KB (' + data.heapBlock + ')</td></tr>';
    }
    if (data.psramMin !== undefined) {
      html += '<tr><td>🧠 PSRAM</td><td>min ' + data.psramMin + 'KB</td></tr>';
    }

    // Timers as component row
    if (data.maxActiveTimers !== undefined) {
      html += '<tr><td>⏱️ Timers</td><td>max ' + data.maxActiveTimers + ' of ' + data.maxTimers + ' used</td></tr>';
    }

    // Version info
    html += '<tr><td>Firmware</td><td>' + (data.firmware || '?') + '</td></tr>';
    html += '<tr><td>WebGUI</td><td>' + (window.KWAL_JS_VERSION || '?') + '</td></tr>';

    html += '</table>';
    container.innerHTML = html;
  }

  function doRestart() {
    if (!confirm('Restart ESP?')) return;
    fetch('/api/restart', { method: 'POST' })
      .then(function(r) {
        if (!r.ok) throw new Error(r.statusText);
        if (sysRestartBtn) sysRestartBtn.textContent = '⏳';
        setTimeout(function() { location.reload(); }, 5000);
      })
      .catch(function(err) {
        alert('Restart failed: ' + err.message);
      });
  }

  function doSleep() {
    if (!confirm('Kwal slaapt tot 06:56. Alleen power off/on kan eerder wekken. Doorgaan?')) return;
    fetch('/api/sleep', { method: 'POST' })
      .then(function(r) {
        if (!r.ok) throw new Error(r.statusText);
        if (sysSleepBtn) sysSleepBtn.textContent = '💤';
      })
      .catch(function(err) {
        alert('Sleep failed: ' + err.message);
      });
  }

  return { init: init, load: load };
})();
