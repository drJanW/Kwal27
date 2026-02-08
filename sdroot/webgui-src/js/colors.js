/*
 * Kwal - Colors module
 * API: GET /api/colors, POST /api/colors/select, POST /api/colors/preview
 */
Kwal.colors = (function() {
  'use strict';

  var listEl, activeId, colorsData = [];
  var editingId = null;
  var originalLabel = null;
  var editingColor = null; // 'a' or 'b'
  var currentA = null, currentB = null;
  var previewTimeout = null;
  var nextBtn, prevBtn;

  function init() {
    listEl = document.getElementById('colors-list');
    nextBtn = document.getElementById('colors-next');
    prevBtn = document.getElementById('colors-prev');
    
    if (nextBtn) {
      nextBtn.onclick = function() { navigate('/api/colors/next'); };
    }
    if (prevBtn) {
      prevBtn.onclick = function() { navigate('/api/colors/prev'); };
    }
  }

  function navigate(url) {
    if (nextBtn) nextBtn.disabled = true;
    if (prevBtn) prevBtn.disabled = true;
    // Clear modified state - switching colors discards unsaved edits
    if (Kwal.state) Kwal.state.clearColorsModified();
    editingId = null;
    currentA = null;
    currentB = null;
    fetch(url, { method: 'POST' })
      .then(function(r) {
        if (r.ok) {
          load();
          // Status labels come from SSE state event
        }
      })
      .finally(function() {
        if (nextBtn) nextBtn.disabled = false;
        if (prevBtn) prevBtn.disabled = false;
      });
  }

  function load() {
    if (!listEl) return;

    fetch('/api/colors')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        activeId = data.active_color;
        colorsData = data.colors || [];
        renderList();
      })
      .catch(function(err) {
        console.error('Colors load failed:', err);
        listEl.innerHTML = '<p style="color:#f88">Laden mislukt</p>';
      });
  }

  function renderList() {
    listEl.innerHTML = '';
    var canDelete = colorsData.length > 1;
    colorsData.forEach(function(c) {
      var item = document.createElement('div');
      item.className = 'color-item' + (c.id === activeId ? ' active' : '');
      
      // Use current editing values if this is the item being edited
      var displayA = (c.id === editingId && currentA) ? currentA : c.colorA_hex;
      var displayB = (c.id === editingId && currentB) ? currentB : c.colorB_hex;
      
      var swatchA = createSwatchBox(c, 'a', displayA);
      var swatchB = createSwatchBox(c, 'b', displayB);
      
      var swatchDiv = document.createElement('div');
      swatchDiv.className = 'color-swatch';
      swatchDiv.appendChild(swatchA);
      swatchDiv.appendChild(swatchB);
      
      var label = document.createElement('span');
      label.className = 'color-label';
      label.textContent = c.label;
      label.onclick = function() { selectColor(c.id); };
      
      item.appendChild(swatchDiv);
      item.appendChild(label);
      
      // Delete button (only if more than 1 color set)
      if (canDelete) {
        var delBtn = document.createElement('button');
        delBtn.className = 'btn-delete';
        delBtn.textContent = '🗑';
        delBtn.title = 'Verwijder';
        delBtn.onclick = function(e) { e.stopPropagation(); deleteColorSet(c.id, c.label); };
        item.appendChild(delBtn);
      }
      
      listEl.appendChild(item);
    });
  }

  function deleteColorSet(id, label) {
    if (!confirm('Verwijder "' + label + '"?')) return;
    fetch('/api/colors/delete', {
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
        activeId = data.active_color;
        colorsData = data.colors || [];
        renderList();
        // Status labels come from SSE state event
      })
      .catch(function(err) {
        console.error('Delete color failed:', err);
        alert('Verwijderen mislukt');
      });
  }

  function selectColor(id) {
    fetch('/api/colors/select', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: id })
    })
      .then(function(r) {
        if (r.ok) {
          activeId = id;
          editingId = null;
          originalLabel = null;
          currentA = null;
          currentB = null;
          renderList();
          // Status labels come from SSE state event
          if (Kwal.state) Kwal.state.clearColorsModified();
        }
      })
      .catch(function() {});
  }

  /**
   * Create a swatch box that opens the custom canvas color picker on tap.
   * Works on all browsers (no <input type="color"> dependency).
   */
  function createSwatchBox(colorSet, which, displayColor) {
    var box = document.createElement('span');
    box.className = 'color-swatch-box';
    box.style.background = displayColor;

    box.onclick = function(e) {
      e.stopPropagation();
      activateEditing(colorSet);
      editingColor = which;
      var startHex = (which === 'a') ? currentA : currentB;
      Kwal.colorpicker.open(startHex, function(hex) {
        if (which === 'a') {
          currentA = hex;
        } else {
          currentB = hex;
        }
        schedulePreview();
        renderList();
        // Mark as modified
        if (Kwal.state) {
          var orig = Kwal.state.getOriginalColors();
          Kwal.state.setColorsModified(true, orig.a, orig.b, orig.id);
        }
      });
    };

    return box;
  }

  function activateEditing(colorSet) {
    if (editingId === colorSet.id) return;
    editingId = colorSet.id;
    originalLabel = colorSet.label;
    currentA = colorSet.colorA_hex;
    currentB = colorSet.colorB_hex;

    // Store original for revert
    if (Kwal.state && !Kwal.state.isColorsModified()) {
      Kwal.state.setColorsModified(false, currentA, currentB, editingId);
    }

    // Select and display
    fetch('/api/colors/select', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: editingId })
    }).then(function() {
      activeId = editingId;
      renderList();
      // Status labels come from SSE state event
    });
  }

  function schedulePreview() {
    if (previewTimeout) clearTimeout(previewTimeout);
    previewTimeout = setTimeout(doPreview, 150);
  }

  function doPreview() {
    if (!currentA || !currentB) return;
    fetch('/api/colors/preview', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ colorA_hex: currentA, colorB_hex: currentB })
    });
  }

  // Called from save modal
  function saveCurrentColors(label) {
    if (!currentA || !currentB) return Promise.reject('No colors');
    var payload = {
      label: label,
      colorA_hex: currentA,
      colorB_hex: currentB,
      select: true
    };
    // Only include id if label unchanged (update existing), omit id to create new
    if (label === originalLabel && editingId) {
      payload.id = editingId;
    }
    return fetch('/api/colors', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    })
      .then(function(r) { return r.json(); })
      .then(function() {
        if (Kwal.state) Kwal.state.clearColorsModified();
        load(); // Refresh list
      });
  }

  function getCurrentLabel() {
    if (!editingId) return '';
    var c = colorsData.find(function(x) { return x.id === editingId; });
    return c ? c.label : '';
  }

  /**
   * Update active color indicator from SSE event (no server call)
   * @param {string} id - Color ID
   */
  function setActiveById(id) {
    if (id && id !== activeId) {
      activeId = id;
      renderList();
    }
  }

  /**
   * Update colors list from SSE event data
   * @param {object} data - {active_color, colors: [...]}
   */
  function updateFromSSE(data) {
    activeId = data.active_color;
    colorsData = data.colors || [];
    editingId = null;
    currentA = null;
    currentB = null;
    renderList();
  }

  return {
    init: init,
    load: load,
    saveCurrentColors: saveCurrentColors,
    getCurrentLabel: getCurrentLabel,
    setActiveById: setActiveById,
    updateFromSSE: updateFromSSE
  };
})();
