/*
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

  function rebuildIndex() {
    if (rebuildMsg) { rebuildMsg.textContent = 'Rebuilding...'; rebuildMsg.className = ''; }
    if (rebuildBtn) rebuildBtn.disabled = true;
    fetch('/api/sd/rebuild', { method: 'POST' })
      .then(function(r) {
        if (!r.ok) throw new Error(r.statusText);
        return r.json();
      })
      .then(function() {
        if (rebuildMsg) { rebuildMsg.textContent = 'Rebuild gestart, check log'; rebuildMsg.className = 'ok'; }
      })
      .catch(function(err) {
        if (rebuildMsg) { rebuildMsg.textContent = 'Error: ' + err.message; rebuildMsg.className = 'err'; }
      })
      .finally(function() {
        if (rebuildBtn) rebuildBtn.disabled = false;
      });
  }

  return {
    init: init
  };
})();
