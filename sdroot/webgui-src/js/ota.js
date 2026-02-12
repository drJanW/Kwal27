/*
 * Kwal - OTA module
 * API: POST /ota/upload (multipart firmware binary)
 */
Kwal.ota = (function() {
  'use strict';

  var fileInput, uploadBtn, status, progressBar;

  function init() {
    fileInput   = document.getElementById('ota-file');
    uploadBtn   = document.getElementById('ota-upload');
    status      = document.getElementById('ota-status');
    progressBar = document.getElementById('ota-progress');

    if (uploadBtn) uploadBtn.onclick = uploadFirmware;
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
      fetch('/api/health', { method: 'GET' })
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

  function setProgress(pct) {
    if (progressBar) {
      progressBar.value = pct;
      progressBar.style.display = pct > 0 ? '' : 'none';
    }
  }

  function resetButtons() {
    if (uploadBtn) uploadBtn.disabled = !fileInput || !fileInput.files.length;
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