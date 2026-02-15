/*
 * Kwal - Pattern module
 * API: GET /api/patterns, POST /api/patterns/select, POST /api/patterns/preview
 */
Kwal.pattern = (function() {
  'use strict';

  var listEl, activeId, patternsData = [];
  var editingId = null;
  var originalLabel = null;
  var currentParams = null;
  var previewTimeout = null;
  var expandedId = null;

  // Parameter definitions with limits from old code
  var PARAMS = [
    { key: 'color_cycle_sec',  label: 'Color Cycle (s)',  min: 1,    max: 120, step: 1 },
    { key: 'bright_cycle_sec', label: 'Bright Cycle (s)', min: 1,    max: 120, step: 1 },
    { key: 'min_brightness',   label: 'Min Brightness',   min: 1,    max: 255, step: 1 },
    { key: 'fade_width',       label: 'Fade Width',       min: 1,    max: 400, step: 0.5 },
    { key: 'gradient_speed',   label: 'Gradient Speed',   min: 0.01, max: 1,   step: 0.01 },
    { key: 'center_x',         label: 'Center X',         min: -132, max: 132, step: 0.5 },
    { key: 'center_y',         label: 'Center Y',         min: -132, max: 132, step: 0.5 },
    { key: 'radius',           label: 'Radius',           min: 1,    max: 164, step: 0.5 },
    { key: 'window_width',     label: 'Window Width',     min: 1,    max: 164, step: 0.5 },
    { key: 'radius_osc',       label: 'Radius Osc',       min: 1,    max: 164, step: 0.5 },
    { key: 'x_amp',            label: 'X Amplitude',      min: 1,    max: 116, step: 0.5 },
    { key: 'y_amp',            label: 'Y Amplitude',      min: 1,    max: 116, step: 0.5 },
    { key: 'x_cycle_sec',      label: 'X Cycle (s)',      min: 1,    max: 120, step: 1 },
    { key: 'y_cycle_sec',      label: 'Y Cycle (s)',      min: 1,    max: 120, step: 1 }
  ];

  var nextBtn, prevBtn;

  function init() {
    listEl = document.getElementById('pattern-list');
    nextBtn = document.getElementById('pattern-next');
    prevBtn = document.getElementById('pattern-prev');
    
    if (nextBtn) {
      nextBtn.onclick = function() { navigate('/api/patterns/next'); };
    }
    if (prevBtn) {
      prevBtn.onclick = function() { navigate('/api/patterns/prev'); };
    }
  }

  function navigate(url) {
    if (nextBtn) nextBtn.disabled = true;
    if (prevBtn) prevBtn.disabled = true;
    // Clear modified state - switching patterns discards unsaved edits
    if (Kwal.state) Kwal.state.clearPatternModified();
    editingId = null;
    currentParams = null;
    expandedId = null;
    fetch(url, { method: 'POST' })
      .then(function(r) {
        if (r.ok) {
          load();
          // Status labels come from SSE state event, no need to call load()
        }
      })
      .finally(function() {
        if (nextBtn) nextBtn.disabled = false;
        if (prevBtn) prevBtn.disabled = false;
      });
  }

  function load() {
    if (!listEl) return;

    fetch('/api/patterns')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        activeId = data.active_pattern;
        patternsData = data.patterns || [];
        renderList();
      })
      .catch(function(err) {
        console.error('Pattern load failed:', err);
        listEl.innerHTML = '<p style="color:#f88">Laden mislukt</p>';
      });
  }

  function renderList() {
    listEl.innerHTML = '';
    var canDelete = patternsData.length > 1;
    patternsData.forEach(function(p) {
      var item = document.createElement('div');
      item.className = 'pattern-item' + (p.id === activeId ? ' active' : '');
      
      var header = document.createElement('div');
      header.className = 'pattern-header';
      
      var label = document.createElement('span');
      label.className = 'pattern-label';
      label.textContent = p.label;
      label.onclick = function() { selectPattern(p.id); };
      
      var editBtn = document.createElement('span');
      editBtn.className = 'pattern-edit-btn';
      editBtn.textContent = '▼';
      editBtn.onclick = function(e) { e.stopPropagation(); toggleExpand(p); };
      
      header.appendChild(label);
      header.appendChild(editBtn);
      
      // Delete button (only if more than 1 pattern)
      if (canDelete) {
        var delBtn = document.createElement('button');
        delBtn.className = 'btn-delete';
        delBtn.textContent = '🗑';
        delBtn.title = 'Verwijder';
        delBtn.onclick = function(e) { e.stopPropagation(); deletePattern(p.id, p.label); };
        header.appendChild(delBtn);
      }
      
      item.appendChild(header);
      
      // Expanded slider panel
      if (expandedId === p.id) {
        var panel = createSliderPanel(p);
        item.appendChild(panel);
      }
      
      listEl.appendChild(item);
    });
  }

  function deletePattern(id, label) {
    if (!confirm('Verwijder "' + label + '"?')) return;
    fetch('/api/patterns/delete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: id })
    })
      .then(function(r) {
        if (r.ok) return r.json();
        throw new Error('Delete failed');
      })
      .then(function(data) {
        // Update from response
        activeId = data.active_pattern;
        patternsData = data.patterns || [];
        editingId = null;
        currentParams = null;
        expandedId = null;
        renderList();
        // Status labels come from SSE state event
      })
      .catch(function(err) {
        console.error('Delete pattern failed:', err);
        alert('Verwijderen mislukt');
      });
  }

  function selectPattern(id) {
    fetch('/api/patterns/select', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: id })
    })
      .then(function(r) {
        if (r.ok) {
          activeId = id;
          editingId = null;
          originalLabel = null;
          currentParams = null;
          expandedId = null;
          renderList();
          // Status labels come from SSE state event
          if (Kwal.state) Kwal.state.clearPatternModified();
        }
      })
      .catch(function() {});
  }

  function toggleExpand(pattern) {
    if (expandedId === pattern.id) {
      expandedId = null;
    } else {
      expandedId = pattern.id;
      editingId = pattern.id;
      originalLabel = pattern.label;
      currentParams = JSON.parse(JSON.stringify(pattern.params));
      
      // Store original for revert
      if (Kwal.state && !Kwal.state.isPatternModified()) {
        Kwal.state.setPatternModified(false, pattern.id);
      }
      
      // Select this pattern
      fetch('/api/patterns/select', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: pattern.id })
      }).then(function() {
        activeId = pattern.id;
        // Status labels come from SSE state event
      });
    }
    renderList();
  }

  function createSliderPanel(pattern) {
    var panel = document.createElement('div');
    panel.className = 'pattern-sliders';
    
    var params = currentParams || pattern.params;
    
    PARAMS.forEach(function(def) {
      var row = document.createElement('div');
      row.className = 'slider-row';
      
      var lbl = document.createElement('label');
      lbl.textContent = def.label;
      
      var isLog = (def.key !== 'gradient_speed');
      var realValue = params[def.key] || def.min;
      
      var slider = document.createElement('input');
      slider.type = 'range';
      slider.min = def.min;
      slider.max = def.max;
      slider.step = def.step;
      slider.value = isLog ? Kwal.valueToSlider(realValue, def.min, def.max) : realValue;
      
      var val = document.createElement('span');
      val.className = 'slider-val';
      
      function formatValue(v) {
        return (def.step < 1) ? Number(v.toFixed(2)) : Math.round(v);
      }
      val.textContent = formatValue(realValue);
      
      slider.oninput = function() {
        var pos = parseFloat(slider.value);
        var real = isLog ? Kwal.sliderToValue(pos, def.min, def.max) : pos;
        // Snap to step
        real = Math.round(real / def.step) * def.step;
        real = Math.max(def.min, Math.min(def.max, real));
        val.textContent = formatValue(real);
        currentParams[def.key] = parseFloat(formatValue(real));
        schedulePreview();
        
        // Mark as modified
        if (Kwal.state) {
          Kwal.state.setPatternModified(true, editingId);
        }
      };
      
      row.appendChild(lbl);
      row.appendChild(slider);
      row.appendChild(val);
      panel.appendChild(row);
    });
    
    return panel;
  }

  function schedulePreview() {
    if (previewTimeout) clearTimeout(previewTimeout);
    previewTimeout = setTimeout(doPreview, 150);
  }

  function doPreview() {
    if (!currentParams) return;
    fetch('/api/patterns/preview', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ params: currentParams })
    });
  }

  // Called from save modal
  function saveCurrentPattern(label) {
    if (!currentParams) return Promise.reject('No params');
    var payload = {
      label: label,
      params: currentParams,
      select: true
    };
    // Only include id if label unchanged (update existing), omit id to create new
    if (label === originalLabel && editingId) {
      payload.id = editingId;
    }
    return fetch('/api/patterns', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    })
      .then(function(r) { return r.json(); })
      .then(function() {
        if (Kwal.state) Kwal.state.clearPatternModified();
        expandedId = null;
        load(); // Refresh list
      });
  }

  function getCurrentLabel() {
    if (!editingId) return '';
    var p = patternsData.find(function(x) { return x.id === editingId; });
    return p ? p.label : '';
  }

  /**
   * Update active pattern indicator from SSE event (no server call)
   * @param {string} id - Pattern ID
   */
  function setActiveById(id) {
    if (id && id !== activeId) {
      activeId = id;
      renderList();
    }
  }

  /**
   * Update patterns list from SSE event data
   * @param {object} data - {active_pattern, patterns: [...]}
   */
  function updateFromSSE(data) {
    activeId = data.active_pattern;
    patternsData = data.patterns || [];
    editingId = null;
    currentParams = null;
    expandedId = null;
    renderList();
  }

  return {
    init: init,
    load: load,
    saveCurrentPattern: saveCurrentPattern,
    getCurrentLabel: getCurrentLabel,
    setActiveById: setActiveById,
    updateFromSSE: updateFromSSE,
    revertPattern: function() {
      var orig = Kwal.state.getOriginalPattern();
      if (orig && orig.id) {
        selectPattern(orig.id);
      }
      if (Kwal.state) Kwal.state.clearPatternModified();
    }
  };
})();
