/*
 * Kwal WebGUI v1214C - Modular
 * Main entry point
 */
(function() {
  'use strict';

  document.addEventListener('DOMContentLoaded', function() {
    Kwal.audio.init();
    Kwal.brightness.init();
    Kwal.modal.init();
    Kwal.sd.init();
    Kwal.pattern.init();
    Kwal.colorpicker.init();
    Kwal.colors.init();
    Kwal.ota.init();
    Kwal.status.init();
    Kwal.health.init();
    Kwal.mp3grid.init();
    
    // Health modal: load on open, refresh button
    var healthModal = document.getElementById('health-modal');
    var healthRefresh = document.getElementById('health-refresh');
    if (healthModal) {
      // Load health when modal becomes visible
      var observer = new MutationObserver(function(mutations) {
        mutations.forEach(function(m) {
          if (m.attributeName === 'class' && healthModal.classList.contains('open')) {
            Kwal.health.load();
          }
        });
      });
      observer.observe(healthModal, { attributes: true });
    }
    if (healthRefresh) {
      healthRefresh.onclick = function() { Kwal.health.load(); };
    }
    
    // MP3 Grid modal: load on first open
    var mp3gridModal = document.getElementById('mp3grid-modal');
    if (mp3gridModal) {
      var mp3gridObserver = new MutationObserver(function(mutations) {
        mutations.forEach(function(m) {
          if (m.attributeName === 'class' && mp3gridModal.classList.contains('open')) {
            Kwal.mp3grid.load();
          }
        });
      });
      mp3gridObserver.observe(mp3gridModal, { attributes: true });
    }
    
    // Initialize SSE and wire up live update listeners
    
    // New unified state callback - primary source of truth
    Kwal.sse.onState(function(data) {
      // Brightness slider uses sliderPct directly
      if (typeof data.sliderPct === 'number' && typeof data.brightnessMax === 'number' && data.brightnessMax > 0) {
        var loPct = 0;
        var hiPct = 100;
        if (typeof data.brightnessLo === 'number') {
          loPct = Math.round((data.brightnessLo / data.brightnessMax) * 100);
        }
        if (typeof data.brightnessHi === 'number') {
          hiPct = Math.round((data.brightnessHi / data.brightnessMax) * 100);
        }
        Kwal.brightness.updateFromState(data.sliderPct, loPct, hiPct);
      }
      // Audio slider: F9 pattern - sliderPct directly from firmware
      if (typeof data.audioSliderPct === 'number' && typeof data.volumeMax === 'number' && data.volumeMax > 0) {
        var loPct = 0;
        var hiPct = 100;
        if (typeof data.volumeLo === 'number') {
          loPct = Math.round((data.volumeLo / data.volumeMax) * 100);
        }
        if (typeof data.volumeHi === 'number') {
          hiPct = Math.round((data.volumeHi / data.volumeMax) * 100);
        }
        Kwal.audio.updateVolumeFromState(data.audioSliderPct, loPct, hiPct);
      }
      // Pattern/color labels (for status display)
      if (Kwal.status.updateFromState) {
        Kwal.status.updateFromState(data.patternLabel, data.colorLabel);
      }
      // Pattern/color selection (for list highlighting)
      if (data.patternId && Kwal.pattern.setActiveById) {
        Kwal.pattern.setActiveById(data.patternId);
      }
      if (data.colorId && Kwal.colors.setActiveById) {
        Kwal.colors.setActiveById(data.colorId);
      }
      // Fragment info
      if (data.fragment) {
        Kwal.audio.updateFragment(data.fragment.dir, data.fragment.file, data.fragment.score, data.fragment.durationMs);
        if (Kwal.mp3grid.setSelection) {
          Kwal.mp3grid.setSelection(data.fragment.dir, data.fragment.file, false);
        }
      }
    });
    
    // Legacy callbacks (still fired by firmware during transition)
    Kwal.sse.onFragment(function(dir, file, score) {
      Kwal.audio.updateFragment(dir, file, score, 0);  // No duration in legacy event
    });
    Kwal.sse.onLight(function(patternId, colorId) {
      // Update pattern/color selection indicators without reloading full lists
      if (Kwal.pattern.setActiveById) Kwal.pattern.setActiveById(patternId);
      if (Kwal.colors.setActiveById) Kwal.colors.setActiveById(colorId);
    });
    Kwal.sse.onColors(function(data) {
      // Update full colors list (triggered by delete)
      if (Kwal.colors.updateFromSSE) Kwal.colors.updateFromSSE(data);
    });
    Kwal.sse.onPatterns(function(data) {
      // Update full patterns list (triggered by delete)
      if (Kwal.pattern.updateFromSSE) Kwal.pattern.updateFromSSE(data);
    });
    Kwal.sse.onReconnect(function() {
      // ESP32 rebooted - SSE pushAll() will send patterns, colors, state
      // Only load() what's NOT covered by SSE state event
      if (Kwal.pattern.load) Kwal.pattern.load();
      if (Kwal.colors.load) Kwal.colors.load();
    });
    Kwal.sse.connect();
    
    // Save colors button on main screen
    var saveColorsBtn = document.getElementById('save-colors-btn');
    var saveColorsConfirm = document.getElementById('save-colors-confirm');
    var saveColorsName = document.getElementById('save-colors-name');
    
    if (saveColorsBtn) {
      saveColorsBtn.onclick = function() {
        if (saveColorsName) {
          saveColorsName.value = Kwal.colors.getCurrentLabel();
        }
        Kwal.modal.open('save-colors-modal');
      };
    }
    
    if (saveColorsConfirm && saveColorsName) {
      saveColorsConfirm.onclick = function() {
        var name = saveColorsName.value.trim();
        if (!name) {
          saveColorsName.style.borderColor = '#f88';
          return;
        }
        saveColorsName.style.borderColor = '#444';
        Kwal.colors.saveCurrentColors(name)
          .then(function() {
            Kwal.modal.close('save-colors-modal');
            if (Kwal.status) Kwal.status.load();
          })
          .catch(function(err) {
            console.error('Save failed:', err);
          });
      };
    }
    
    // Save pattern button on main screen
    var savePatternBtn = document.getElementById('save-pattern-btn');
    var savePatternConfirm = document.getElementById('save-pattern-confirm');
    var savePatternName = document.getElementById('save-pattern-name');
    
    if (savePatternBtn) {
      savePatternBtn.onclick = function() {
        if (savePatternName) {
          savePatternName.value = Kwal.pattern.getCurrentLabel();
        }
        Kwal.modal.open('save-pattern-modal');
      };
    }
    
    if (savePatternConfirm && savePatternName) {
      savePatternConfirm.onclick = function() {
        var name = savePatternName.value.trim();
        if (!name) {
          savePatternName.style.borderColor = '#f88';
          return;
        }
        savePatternName.style.borderColor = '#444';
        Kwal.pattern.saveCurrentPattern(name)
          .then(function() {
            Kwal.modal.close('save-pattern-modal');
            if (Kwal.status) Kwal.status.load();
          })
          .catch(function(err) {
            console.error('Save failed:', err);
          });
      };
    }
  });

})();
