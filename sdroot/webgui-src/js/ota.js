/*
 * Kwal - OTA module
 * API: POST /ota/upload (multipart firmware binary)
 *       POST /ota/start  (legacy ArduinoOTA arm)
 */
Kwal.ota = (function() {
  'use strict';

  var fileInput, uploadBtn, status, progressBar, startBtn;

  function init() {
    fileInput   = document.getElementById('ota-file');
    uploadBtn   = document.getElementById('ota-upload');
    status      = document.getElementById('ota-status');
    progressBar = document.getElementById('ota-progress');
    startBtn    = document.getElementById('ota-start');

    if (uploadBtn) uploadBtn.onclick = uploadFirmware;
    if (startBtn)  startBtn.onclick  = startOta;
    if (fileInput) fileInput.onchange = function() {
      if (uploadBtn) uploadBtn.disabled = !fileInput.files.length;
      showStatus('', false);
    };
  }

  function uploadFirmware() {
    if (!fileInput || !fileInput.files.length) {
      showStatus('Kies eerst een .bin bestand', true);
      return;
    }
    var file = fileInput.files[0];
    if (file.size < 10000) {
      showStatus('Bestand te klein - kies firmware .bin', true);
      return;
    }

    if (uploadBtn) uploadBtn.disabled = true;
    if (startBtn)  startBtn.disabled  = true;
    showStatus('Uploading ' + file.name + ' (' + formatSize(file.size) + ')...', false);
    setProgress(0);

    var formData = new FormData();
    formData.append('firmware', file, file.name);

    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/ota/upload', true);

    xhr.upload.onprogress = function(e) {
      if (e.lengthComputable) {
        var pct = Math.round(e.loaded / e.total * 100);
        setProgress(pct);
        showStatus('Uploading... ' + pct + '%', false);
      }
    };

    xhr.onload = function() {
      if (xhr.status === 200) {
        setProgress(100);
        showStatus('Upload OK! Rebooting...', false);
        setTimeout(waitForReboot, 5000);
      } else {
        var errMsg = 'Upload mislukt';
        try { errMsg = JSON.parse(xhr.responseText).error || errMsg; } catch(e) {}
        showStatus(errMsg, true);
        resetButtons();
      }
    };

    xhr.onerror = function() {
      showStatus('Verbinding mislukt', true);
      resetButtons();
    };

    xhr.send(formData);
  }

  function waitForReboot() {
    var attempts = 0;
    var maxAttempts = 30;
    showStatus('Wachten op herstart...', false);

    var timer = setInterval(function() {
      attempts++;
      fetch('/ota/arm', { method: 'GET' })
        .then(function(r) {
          if (r.ok) {
            clearInterval(timer);
            showStatus('Herstart gelukt! Pagina ververst...', false);
            setTimeout(function() { location.reload(); }, 1500);
          }
        })
        .catch(function() {
          if (attempts >= maxAttempts) {
            clearInterval(timer);
            showStatus('Timeout - ververs pagina handmatig', true);
            resetButtons();
          }
        });
    }, 2000);
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
    var countdown = seconds;
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

  function setProgress(pct) {
    if (progressBar) {
      progressBar.value = pct;
      progressBar.style.display = pct > 0 ? '' : 'none';
    }
  }

  function resetButtons() {
    if (uploadBtn) uploadBtn.disabled = !fileInput || !fileInput.files.length;
    if (startBtn)  startBtn.disabled  = false;
    setProgress(0);
  }

  function showStatus(msg, isError) {
    if (status) {
      status.textContent = msg;
      status.className = isError ? 'err' : '';
    }
  }

  function formatSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(1) + ' MB';
  }

  return { init: init };
})();