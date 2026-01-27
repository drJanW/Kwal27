/*
 * Kwal - SD module (simple upload to root)
 */
Kwal.sd = (function() {
  'use strict';

  var fileInput, uploadBtn, uploadMsg;

  function init() {
    fileInput = document.getElementById('upload-file');
    uploadBtn = document.getElementById('upload-btn');
    uploadMsg = document.getElementById('upload-msg');

    if (uploadBtn) uploadBtn.onclick = upload;
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

  return {
    init: init
  };
})();
