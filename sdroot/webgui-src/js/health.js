/*
 * Kwal - Health module
 * System health status display in DEV modal
 * API: GET /api/health, POST /api/restart
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

  var container, restartBtn;

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
    restartBtn = document.getElementById('restart-btn');
    if (restartBtn) {
      restartBtn.onclick = doRestart;
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

    var html = '<table class="health-table">';

    // Version info
    html += '<tr><th colspan="2">Versions</th></tr>';
    html += '<tr><td>Firmware</td><td>' + (data.firmware || '?') + '</td></tr>';
    html += '<tr><td>WebGUI</td><td>' + (window.KWAL_JS_VERSION || '?') + '</td></tr>';

    // Timer count
    html += '<tr><th colspan="2">Resources</th></tr>';
    html += '<tr><td>Timers</td><td>' + data.timers + '/' + data.maxTimers + '</td></tr>';

    // Flags with boot status (use boot field if available, fallback to health bits)
    html += '<tr><th colspan="2">Hardware & Status</th></tr>';
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
      // Append calendar date after Calendar status
      if (f.name === 'Calendar' && data.calendarDate) {
        status += ' ' + data.calendarDate;
      }
      html += '<tr><td>' + f.icon + ' ' + f.name + '</td><td>' + status + '</td></tr>';
    }

    html += '</table>';
    container.innerHTML = html;
  }

  function doRestart() {
    if (restartBtn) restartBtn.disabled = true;
    fetch('/api/restart', { method: 'POST' })
      .then(function(r) {
        if (!r.ok) throw new Error(r.statusText);
        if (restartBtn) restartBtn.textContent = 'Restarting...';
        setTimeout(function() { location.reload(); }, 5000);
      })
      .catch(function(err) {
        alert('Restart failed: ' + err.message);
        if (restartBtn) restartBtn.disabled = false;
      });
  }

  return { init: init, load: load };
})();
