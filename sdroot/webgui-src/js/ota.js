/*
 * Kwal - OTA module
 * API: POST /ota/start
 */
Kwal.ota = (function() {
  'use strict';

  var startBtn, status, countdown;

  function init() {
    startBtn = document.getElementById('ota-start');
    status = document.getElementById('ota-status');

    if (startBtn) {
      startBtn.onclick = startOta;
    }
  }

  function startOta() {
    if (startBtn) startBtn.disabled = true;
    showStatus('OTA starten...', false);

    fetch('/ota/start', { method: 'POST' })
      .then(function(r) {
        if (!r.ok) throw new Error(r.statusText);
        return r.text();
      })
      .then(function(msg) {
        showStatus(msg, false);
        startCountdown(300);
      })
      .catch(function(err) {
        showStatus('Error: ' + err.message, true);
        if (startBtn) startBtn.disabled = false;
      });
  }

  function startCountdown(seconds) {
    countdown = seconds;
    showStatus('Waiting for upload... ' + countdown + 's', false);
    var timer = setInterval(function() {
      countdown--;
      if (countdown <= 0) {
        clearInterval(timer);
        showStatus('OTA timeout - refresh page', false);
        if (startBtn) startBtn.disabled = false;
      } else {
        showStatus('Waiting for upload... ' + countdown + 's', false);
      }
    }, 1000);
  }

  function showStatus(msg, isError) {
    if (status) {
      status.textContent = msg;
      status.className = isError ? 'err' : '';
    }
  }

  return { init: init };
})();
