(function() {
  'use strict';
  
  function fetchLog() {
    var pre = document.getElementById('log-content');
    if (!pre) return;
    
    fetch('/log')
      .then(function(r) { return r.text(); })
      .then(function(txt) { 
        pre.textContent = txt || '(empty)';
        pre.scrollTop = pre.scrollHeight;
      })
      .catch(function(e) { pre.textContent = 'Error: ' + e; });
  }
  
  function clearLog() {
    fetch('/log/clear')
      .then(function() { fetchLog(); })
      .catch(function(e) { console.error('Clear failed:', e); });
  }
  
  document.addEventListener('DOMContentLoaded', function() {
    var refreshBtn = document.getElementById('log-refresh');
    var clearBtn = document.getElementById('log-clear');
    
    if (refreshBtn) refreshBtn.onclick = fetchLog;
    if (clearBtn) clearBtn.onclick = clearLog;
    
    // Auto-fetch when modal opens
    var logModal = document.getElementById('log-modal');
    if (logModal) {
      var observer = new MutationObserver(function(mutations) {
        mutations.forEach(function(m) {
          if (m.attributeName === 'class' && 
              logModal.classList.contains('open')) {
            fetchLog();
          }
        });
      });
      observer.observe(logModal, { attributes: true });
    }
  });
})();
