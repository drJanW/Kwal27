/**
 * @file    sd.js
 * @version 260312A
 * @date    2026-03-12
 *
 * Kwal - SD module (upload + index rebuild)
 */
Kwal.sd = (function() {
  'use strict';

  var fileInput, uploadBtn, uploadMsg, rebuildBtn, rebuildMsg;

  function init() {
    fileInput = document.getElementById('upload-file');
    uploadBtn = document.getElementById('upload-btn');
    uploadMsg = document.getElementById('upload-msg');
    rebuildBtn = document.getElementById('rebuild-idx-btn');
    rebuildMsg = document.getElementById('rebuild-msg');

    if (uploadBtn) uploadBtn.onclick = upload;
    if (rebuildBtn) rebuildBtn.onclick = rebuildIndex;

    // Check index status when SD modal becomes visible
    var sdModal = document.getElementById('sd-modal');
    if (sdModal) {
      new MutationObserver(function() {
        if (sdModal.classList.contains('active')) checkIndexStatus();
      }).observe(sdModal, { attributes: true, attributeFilter: ['class'] });
    }
  }

  function checkIndexStatus() {
    fetch('/api/sd/status')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (data.indexDirty) {
          if (rebuildMsg) { rebuildMsg.textContent = 'Index verouderd'; rebuildMsg.className = 'err'; }
        }
      })
      .catch(function() {});
  }

  function upload() {
    if (!fileInput || !fileInput.files[0]) {
      showMsg('No file selected', true);
      return;
    }

    var file = fileInput.files[0];
    showMsg('Uploading...', false);

    var form = new FormData();
    form.append('file', file);

    fetch('/api/sd/upload', { method: 'POST', body: form })
      .then(function(r) {
        if (!r.ok) throw new Error(r.statusText);
        return r.json();
      })
      .then(function(data) {
        showMsg('OK: ' + (data.path || file.name), false);
        fileInput.value = '';
      })
      .catch(function(err) {
        showMsg('Error: ' + err.message, true);
      });
  }

  function showMsg(text, isError) {
    if (uploadMsg) {
      uploadMsg.textContent = text;
      uploadMsg.className = isError ? 'err' : 'ok';
    }
  }

  function pollRebuildDone() {
    fetch('/api/sd/status')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (data.busy) {
          setTimeout(pollRebuildDone, 2000);
        } else {
          if (rebuildMsg) { rebuildMsg.textContent = 'Rebuild klaar'; rebuildMsg.className = 'ok'; }
          if (rebuildBtn) rebuildBtn.disabled = false;
          if (Kwal.mp3grid && Kwal.mp3grid.reload) Kwal.mp3grid.reload();
        }
      })
      .catch(function() {
        setTimeout(pollRebuildDone, 2000);
      });
  }

  function rebuildIndex() {
    if (rebuildMsg) { rebuildMsg.textContent = 'Rebuilding...'; rebuildMsg.className = ''; }
    if (rebuildBtn) rebuildBtn.disabled = true;
    fetch('/api/sd/rebuild', { method: 'POST' })
      .then(function(r) {
        if (!r.ok) throw new Error(r.statusText);
        return r.json();
      })
      .then(function() {
        setTimeout(pollRebuildDone, 2000);
      })
      .catch(function(err) {
        if (rebuildMsg) { rebuildMsg.textContent = 'Error: ' + err.message; rebuildMsg.className = 'err'; }
        if (rebuildBtn) rebuildBtn.disabled = false;
      });
  }

  return {
    init: init
  };
})();
