/*
 * Kwal - Audio module
 * See docs/glossary_slider_semantics.md for terminology
 * 
 * Slider moves freely 0-100%. Grey zones show shiftedLo/Hi
 * as visual indicators but do NOT restrict the thumb.
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
  var pctMin = 0;     // Slider minimum
  var pctMax = 100;   // Slider maximum
  var loPct = 0;      // Grey zone left boundary (visual only)
  var hiPct = 100;    // Grey zone right boundary (visual only)

  // Interval control elements
  var muteBtn, speakSlider, fragSlider, durSlider;
  var speakLabel, fragLabel, durLabel;
  var debounceTimer = null;

  // Non-linear step tables (logarithmic distribution)
  var speakSteps = [1,2,3,5,10,15,20,30,45,60,90,120,180,240,360,480,720];
  var fragSteps  = [2,3,5,10,15,20,30,45,60,90,120,180,240,360,480,720];
  var durSteps   = [5,10,15,30,45,60,120,240,360,480,720,780];

  function formatMinutes(min) {
    if (min < 60) return min + 'm';
    var h = Math.floor(min / 60);
    var m = min % 60;
    return m === 0 ? h + 'u' : h + 'u' + m;
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
        var pos = Math.max(pctMin, Math.min(pctMax, parseInt(slider.value, 10)));
        slider.value = pos;
        label.textContent = pos + '%';
      };

      slider.onchange = function() {
        var pos = Math.max(pctMin, Math.min(pctMax, parseInt(slider.value, 10)));
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
          fetch('/api/audio/play?dir=' + currentDir + '&src=dir%2B').catch(function() {
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
          fetch('/api/audio/play?dir=' + currentDir + '&file=' + currentFile + '&src=replay').catch(function() {
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
    initIntervalControls();
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
      var pos = Math.max(pctMin, Math.min(pctMax, Math.round(sliderPct)));
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

  // ─── Interval / silence controls ──────────────────────────

  function initIntervalControls() {
    muteBtn = document.getElementById('audio-mute');
    speakSlider = document.getElementById('speak-interval');
    fragSlider = document.getElementById('frag-interval');
    durSlider = document.getElementById('interval-duration');
    speakLabel = document.getElementById('speak-num');
    fragLabel = document.getElementById('frag-num');
    durLabel = document.getElementById('dur-num');

    // Silence toggle — instant
    if (muteBtn) {
      muteBtn.onclick = function() {
        var isMuted = muteBtn.classList.toggle('muted');
        muteBtn.textContent = isMuted ? '🔇' : '🔊';
        fetch('/api/audio/silence?active=' + (isMuted ? '1' : '0'),
              {method:'POST'}).catch(function(){});
      };
    }

    function bindSlider(sl, steps, lbl) {
      if (!sl || !lbl) return;
      sl.oninput = function() {
        var val = steps[parseInt(sl.value, 10)];
        lbl.textContent = formatMinutes(val);
        scheduleIntervalSend();
      };
    }
    bindSlider(speakSlider, speakSteps, speakLabel);
    bindSlider(fragSlider, fragSteps, fragLabel);
    bindSlider(durSlider, durSteps, durLabel);
  }

  function scheduleIntervalSend() {
    if (debounceTimer) clearTimeout(debounceTimer);
    debounceTimer = setTimeout(sendIntervals, 3000);
  }

  function sendIntervals() {
    var speakMin = speakSteps[parseInt(speakSlider.value, 10)];
    var fragMin  = fragSteps[parseInt(fragSlider.value, 10)];
    var durMin   = durSteps[parseInt(durSlider.value, 10)];
    var url = '/api/audio/intervals'
      + '?speak=' + speakMin
      + '&frag=' + fragMin
      + '&dur=' + durMin;
    fetch(url, {method:'POST'})
      .then(function() { flashConfirm(); })
      .catch(function(){});
  }

  function flashConfirm() {
    [speakLabel, fragLabel, durLabel].forEach(function(el) {
      if (!el) return;
      var orig = el.textContent;
      el.textContent = '✓';
      el.style.color = '#4f4';
      setTimeout(function() {
        el.textContent = orig;
        el.style.color = '';
      }, 1000);
    });
  }

  function updateIntervalsFromState(data) {
    if (typeof data.silence === 'boolean' && muteBtn) {
      muteBtn.textContent = data.silence ? '🔇' : '🔊';
      muteBtn.classList.toggle('muted', data.silence);
    }
    function findStep(steps, val) {
      for (var i = steps.length - 1; i >= 0; i--) {
        if (steps[i] <= val) return i;
      }
      return 0;
    }
    if (typeof data.speakMin === 'number' && speakSlider && speakLabel) {
      speakSlider.value = findStep(speakSteps, data.speakMin);
      speakLabel.textContent = formatMinutes(data.speakMin);
    }
    if (typeof data.fragMin === 'number' && fragSlider && fragLabel) {
      fragSlider.value = findStep(fragSteps, data.fragMin);
      fragLabel.textContent = formatMinutes(data.fragMin);
    }
    if (typeof data.durMin === 'number' && durSlider && durLabel) {
      durSlider.value = findStep(durSteps, data.durMin);
      durLabel.textContent = formatMinutes(data.durMin);
    }
  }

  return {
    init: init,
    updateVolumeFromState: updateVolumeFromState,
    updateFragment: updateFragment,
    updateIntervalsFromState: updateIntervalsFromState
  };
})();
