/**
 * @file    modal.js
 * @version 260312A
 * @date    2026-03-12
 *
 * Kwal - Modal module
 */
Kwal.modal = (function() {
  'use strict';

  var backdropLocked = false;

  function init() {
    // Dev button opens dev modal
    var devBtn = document.getElementById('dev-btn');
    if (devBtn) {
      devBtn.onclick = function() { open('dev-modal'); };
    }

    // Buttons with data-open attribute
    var openBtns = document.querySelectorAll('[data-open]');
    for (var i = 0; i < openBtns.length; i++) {
      (function(btn) {
        btn.onclick = function() {
          // Close parent modal (if any)
          var parentModal = btn.closest('.modal');
          if (parentModal) parentModal.classList.remove('open');
          var targetId = btn.getAttribute('data-open');
          open(targetId);
          // Trigger load for specific modals
          if (targetId === 'pattern-modal') Kwal.pattern.load();
          if (targetId === 'colors-modal') Kwal.colors.load();
        };
      })(openBtns[i]);
    }

    // Buttons with data-close attribute
    var closeBtns = document.querySelectorAll('[data-close]');
    for (var j = 0; j < closeBtns.length; j++) {
      (function(btn) {
        btn.onclick = function() {
          var modal = btn.closest('.modal');
          if (modal) modal.classList.remove('open');
          // Return to parent modal if specified
          var returnTo = btn.getAttribute('data-return');
          if (returnTo) open(returnTo);
        };
      })(closeBtns[j]);
    }

    // Click on backdrop closes modal (unless locked)
    var modals = document.querySelectorAll('.modal');
    for (var k = 0; k < modals.length; k++) {
      (function(m) {
        m.onclick = function(e) {
          if (e.target === m && !backdropLocked) m.classList.remove('open');
        };
      })(modals[k]);
    }
  }

  function open(id) {
    var el = document.getElementById(id);
    if (el) el.classList.add('open');
  }

  function close(id) {
    var el = document.getElementById(id);
    if (el) el.classList.remove('open');
  }

  function lockBackdrop() { backdropLocked = true; }
  function unlockBackdrop() { backdropLocked = false; }

  return { init: init, open: open, close: close, lockBackdrop: lockBackdrop, unlockBackdrop: unlockBackdrop };
})();
