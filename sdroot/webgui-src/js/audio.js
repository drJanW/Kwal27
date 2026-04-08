/**
 * @file    audio.js
 * @version 260407E
 * @date    2026-04-07
 *
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
  var voteDirtyUntil = 0;
  var currentDir = null, currentFile = null;
  var isPlaying = false;
  var playingTimeout = null;
  var pctMin = 0;     // Slider minimum
  var pctMax = 100;   // Slider maximum
  var loPct = 0;      // Grey zone left boundary (visual only)
  var hiPct = 100;    // Grey zone right boundary (visual only)

  // Interval control elements
  var muteBtn, speakSlider, fragSlider, durSlider, lightDurSlider;
  var speakLabel, fragLabel, durLabel, lightDurLabel;
  var debounceTimer = null;

  // Free-text TTS elements
  var freetextInput, freetextPlayBtn, freetextStopBtn;
  var freetextIntervalSlider, freetextIntervalLabel;
  var freetextIntervalSteps = [1,2,3,5,10,15,30,60];

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
    // Hide audio panel + settings modal if hardware absent (bit 3 = SC_AUDIO)
    fetch('/api/health').then(function(r) { return r.json(); }).then(function(data) {
      if ((data.absent || 0) & (1 << 3)) {
        var panel = document.getElementById('audio-panel');
        if (panel) panel.style.display = 'none';
        var modal = document.getElementById('audio-settings-modal');
        if (modal) modal.remove();
      }
    }).catch(function() {});

    slider = document.getElementById('volume');
    label = document.getElementById('vol-num');
    nextBtn = document.getElementById('audio-next');
    dirEl = document.getElementById('audio-dir');
    fileEl = document.getElementById('audio-file');
    boxLabelEl = document.getElementById('audio-box-label');
    voteUpBtn = document.getElementById('vote-up');
    voteDownBtn = document.getElementById('vote-down');
    voteScoreEl = document.getElementById('vote-score');
    var voteMinBtn = document.getElementById('vote-min');
    var voteMaxBtn = document.getElementById('vote-max');
    
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

    // Click box label: play random fragment from another dir in same theme box
    if (boxLabelEl) {
      boxLabelEl.onclick = function() {
        if (currentDir !== null && !isPlaying && boxLabelEl.textContent !== 'Audio') {
          setPlayingState(true);
          if (playingTimeout) clearTimeout(playingTimeout);
          playingTimeout = setTimeout(function() { setPlayingState(false); }, 32000);
          fetch('/api/audio/playbox?dir=' + currentDir).catch(function() {
            setPlayingState(false);
          });
        }
      };
      boxLabelEl.classList.add('clickable');
    }

    if (voteUpBtn) {
      voteUpBtn.onclick = function() { vote(3); };
    }
    if (voteDownBtn) {
      voteDownBtn.onclick = function() { vote(-5); };
    }
    if (voteMinBtn) {
      voteMinBtn.onclick = function() { voteSet(10); };
    }
    if (voteMaxBtn) {
      voteMaxBtn.onclick = function() { voteSet(200); };
    }
    if (voteScoreEl) {
      voteScoreEl.onclick = function() { voteSet(100); };
      voteScoreEl.classList.add('clickable');
    }
    
    // No load() - initial state comes from SSE
    initIntervalControls();
  }

  function vote(delta) {
    voteDirtyUntil = Date.now() + 3000;
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

  function voteSet(score) {
    voteDirtyUntil = Date.now() + 3000;
    if (voteScoreEl) voteScoreEl.textContent = String(score);
    fetch('/vote?set=' + score, { method: 'POST' }).catch(function() {});
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
    if (typeof score === 'number' && voteScoreEl && Date.now() > voteDirtyUntil) {
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
    lightDurSlider = document.getElementById('light-duration');
    speakLabel = document.getElementById('speak-num');
    fragLabel = document.getElementById('frag-num');
    durLabel = document.getElementById('dur-num');
    lightDurLabel = document.getElementById('light-dur-num');

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

    // Light-duration slider mirrors audio duration
    if (lightDurSlider && lightDurLabel) {
      lightDurSlider.oninput = function() {
        var val = durSteps[parseInt(lightDurSlider.value, 10)];
        lightDurLabel.textContent = formatMinutes(val);
        // Sync audio slider
        if (durSlider) durSlider.value = lightDurSlider.value;
        if (durLabel) durLabel.textContent = formatMinutes(val);
        scheduleIntervalSend();
      };
    }
    // When audio dur slider changes, also sync light dur slider
    if (durSlider && lightDurSlider) {
      var origDurInput = durSlider.oninput;
      durSlider.oninput = function() {
        if (origDurInput) origDurInput.call(durSlider);
        lightDurSlider.value = durSlider.value;
        if (lightDurLabel) lightDurLabel.textContent = durLabel.textContent;
      };
    }

    // Free-text TTS controls
    initFreeTextControls();
  }

  function initFreeTextControls() {
    freetextInput = document.getElementById('freetext-input');
    freetextPlayBtn = document.getElementById('freetext-play');
    freetextStopBtn = document.getElementById('freetext-stop');
    freetextIntervalSlider = document.getElementById('freetext-interval');
    freetextIntervalLabel = document.getElementById('freetext-interval-num');

    if (freetextIntervalSlider && freetextIntervalLabel) {
      freetextIntervalSlider.oninput = function() {
        var val = freetextIntervalSteps[parseInt(freetextIntervalSlider.value, 10)];
        freetextIntervalLabel.textContent = formatMinutes(val);
      };
    }

    if (freetextPlayBtn) {
      freetextPlayBtn.onclick = function() {
        if (Kwal.ttsOk === false) return;
        var text = freetextInput ? freetextInput.value.trim() : '';
        if (!text) {
          sendFreeTextClear();
          return;
        }
        var intervalMin = freetextIntervalSteps[parseInt(freetextIntervalSlider.value, 10)];
        var durMin = durSteps[parseInt(durSlider.value, 10)];
        var url = '/api/audio/freetext?text=' + encodeURIComponent(text)
          + '&interval=' + intervalMin
          + '&dur=' + durMin;
        fetch(url, {method:'POST'}).catch(function(){});
      };
    }

    if (freetextStopBtn) {
      freetextStopBtn.onclick = function() {
        sendFreeTextClear();
      };
    }
  }

  function sendFreeTextClear() {
    if (freetextInput) freetextInput.value = '';
    fetch('/api/audio/freetext/clear', {method:'POST'}).catch(function(){});
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
    [speakLabel, fragLabel, durLabel, lightDurLabel].forEach(function(el) {
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
    if (typeof data.durMin === 'number' && lightDurSlider && lightDurLabel) {
      lightDurSlider.value = findStep(durSteps, data.durMin);
      lightDurLabel.textContent = formatMinutes(data.durMin);
    }
    if (freetextInput) {
      freetextInput.value = (typeof data.freeText === 'string') ? data.freeText : '';
    }
  }

  function updateTtsState() {
    var disabled = (Kwal.ttsOk === false);
    var opacity = disabled ? '0.3' : '1';
    if (freetextInput) { freetextInput.disabled = disabled; freetextInput.style.opacity = opacity; }
    if (freetextPlayBtn) freetextPlayBtn.style.opacity = opacity;
  }

  return {
    init: init,
    updateVolumeFromState: updateVolumeFromState,
    updateFragment: updateFragment,
    updateIntervalsFromState: updateIntervalsFromState,
    updateTtsState: updateTtsState
  };
})();
