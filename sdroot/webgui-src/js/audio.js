/*
 * Kwal - Audio module
 * See docs/glossary_slider_semantics.md for terminology
 * 
 * F9 pattern: Slider shows sliderPct (0-100%) directly.
 * Grey zone left: 0% to loPct
 * Blue zone: loPct to hiPct (usable range)
 * Grey zone right: hiPct to 100%
 * 
 * sliderPct = current volume as percentage of Lo..Hi range
 */
Kwal.audio = (function() {
  'use strict';

  var slider, label, nextBtn, dirEl, fileEl, boxLabelEl;
  var voteUpBtn, voteDownBtn, voteScoreEl;
  var currentDir = null, currentFile = null;
  var isPlaying = false;
  var playingTimeout = null;
  var loPct = 0;      // Left grey zone boundary (%)
  var hiPct = 100;    // Right grey zone boundary (%)

  function clamp(val) {
    return Math.max(loPct, Math.min(hiPct, val));
  }

  function updateGradient() {
    if (!slider) return;
    var style = 'linear-gradient(to right, ' +
      '#555 0%, #555 ' + loPct + '%, ' +
      '#4682B4 ' + loPct + '%, #4682B4 ' + hiPct + '%, ' +
      '#555 ' + hiPct + '%, #555 100%)';
    slider.style.background = style;
  }

  function setPlayingState(playing) {
    isPlaying = playing;
    if (dirEl) dirEl.classList.toggle('disabled', playing);
    if (fileEl) fileEl.classList.toggle('disabled', playing);
  }

  function init() {
    slider = document.getElementById('volume');
    label = document.getElementById('vol-num');
    nextBtn = document.getElementById('audio-next');
    dirEl = document.getElementById('audio-dir');
    fileEl = document.getElementById('audio-file');
    boxLabelEl = document.getElementById('audio-box-label');
    voteUpBtn = document.getElementById('vote-up');
    voteDownBtn = document.getElementById('vote-down');
    voteScoreEl = document.getElementById('vote-score');
    
    if (slider && label) {
      slider.oninput = function() {
        var pos = clamp(parseInt(slider.value, 10));
        slider.value = pos;
        label.textContent = pos + '%';
      };

      slider.onchange = function() {
        var pos = clamp(parseInt(slider.value, 10));
        slider.value = pos;
        label.textContent = pos + '%';
        // Send linear value - firmware calculates webMultiplier
        fetch('/setWebAudioLevel?value=' + pos, { method: 'POST' }).catch(function() {});
      };
      
      updateGradient();
    }

    if (nextBtn) {
      nextBtn.onclick = function() {
        nextBtn.disabled = true;
        fetch('/api/audio/next', { method: 'POST' })
          .then(function() {
            setTimeout(function() {
              nextBtn.disabled = false;
            }, 500);
          })
          .catch(function() {
            nextBtn.disabled = false;
          });
      };
    }

    // Click dir: play random from same dir
    if (dirEl) {
      dirEl.onclick = function() {
        if (currentDir !== null && !isPlaying) {
          setPlayingState(true);
          if (playingTimeout) clearTimeout(playingTimeout);
          playingTimeout = setTimeout(function() { setPlayingState(false); }, 32000);
          fetch('/api/audio/play?dir=' + currentDir).catch(function() {
            setPlayingState(false);
          });
        }
      };
    }

    // Click file: replay exact fragment
    if (fileEl) {
      fileEl.onclick = function() {
        if (currentDir !== null && currentFile !== null && !isPlaying) {
          setPlayingState(true);
          if (playingTimeout) clearTimeout(playingTimeout);
          playingTimeout = setTimeout(function() { setPlayingState(false); }, 32000);
          fetch('/api/audio/play?dir=' + currentDir + '&file=' + currentFile).catch(function() {
            setPlayingState(false);
          });
        }
      };
    }

    if (voteUpBtn) {
      voteUpBtn.onclick = function() { vote(3); };
    }
    if (voteDownBtn) {
      voteDownBtn.onclick = function() { vote(-5); };
    }
    
    // No load() - initial state comes from SSE
  }

  function vote(delta) {
    // Optimistic UI - update score immediately, fire-and-forget
    if (voteScoreEl) {
      var current = parseInt(voteScoreEl.textContent, 10);
      if (!isNaN(current)) {
        var newScore = Math.max(1, Math.min(200, current + delta));
        voteScoreEl.textContent = String(newScore);
      }
    }
    // Fire and forget - no blocking
    fetch('/vote?delta=' + delta, { method: 'POST' }).catch(function() {});
  }

  /**
   * Update volume slider from SSE state event
   * @param {number} sliderPct Current volume as percentage (0-100)
   * @param {number} loPercent Left grey zone boundary (%)
   * @param {number} hiPercent Right grey zone boundary (%)
   */
  function updateVolumeFromState(sliderPct, loPercent, hiPercent) {
    if (typeof loPercent === 'number') loPct = loPercent;
    if (typeof hiPercent === 'number') hiPct = hiPercent;
    updateGradient();
    if (slider && label && typeof sliderPct === 'number') {
      var pos = clamp(Math.round(sliderPct));
      slider.value = pos;
      label.textContent = pos + '%';
    }
  }

  /**
   * Update fragment display from SSE event
   * @param {number} dir 
   * @param {number} file 
   * @param {number} score
   * @param {number} durationMs Fragment duration in ms (0 = use default)
   */
  function updateFragment(dir, file, score, durationMs, boxName) {
    var isFirstLoad = (currentDir === null);
    var isNewFragment = (dir !== currentDir || file !== currentFile);
    currentDir = dir;
    currentFile = file;
    
    // Only set playing state if this is a NEW fragment during runtime (not first SSE connect)
    if (!isFirstLoad && isNewFragment && dir > 0 && file > 0) {
      setPlayingState(true);
      if (playingTimeout) clearTimeout(playingTimeout);
      // Use durationMs from SSE, fallback to 32s if not available
      var timeout = (typeof durationMs === 'number' && durationMs > 0) ? durationMs + 200 : 32000;
      playingTimeout = setTimeout(function() { setPlayingState(false); }, timeout);
    }
    
    if (dirEl) {
      dirEl.textContent = String(dir).padStart(3, '0');
    }
    if (fileEl) {
      fileEl.textContent = String(file).padStart(3, '0');
    }
    if (typeof score === 'number' && voteScoreEl) {
      voteScoreEl.textContent = score;
    }
    if (boxLabelEl && typeof boxName === 'string' && boxName.length > 0) {
      boxLabelEl.textContent = boxName;
    }
  }

  return { init: init, updateVolumeFromState: updateVolumeFromState, updateFragment: updateFragment };
})();
