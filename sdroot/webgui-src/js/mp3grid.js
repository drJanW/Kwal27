/*
 * Kwal - MP3 Grid module
 * Canvas grid showing dirs from root_dirs index + theme box colors.
 * One-shot load via /api/audio/grid; crosshair follows SSE fragment events.
 */
Kwal.mp3grid = (function() {
  'use strict';

  var COLS = 101;   // file 1-100 (column 0 unused)
  var ROWS = 200;   // dir 1-199
  var CELL = 5;
  var W = COLS * CELL;
  var H = ROWS * CELL;

  var canvas, ctx, wrap;
  var dirSlider, fileSlider, dirVal, fileVal;
  var catpill, catlabel, closeBtn;

  var selRow = 0, selCol = 0;
  var loaded = false;

  // Data from /api/audio/grid
  var boxById = {};           // id → {name, color}
  var dirBox = null;          // Uint8Array[ROWS] dir → box id
  var dirFileCount = null;    // Uint16Array[ROWS] dir → number of files

  function hexToRgb(hex) {
    var h = hex.replace('#', '');
    if (h.length === 3) h = h[0]+h[0]+h[1]+h[1]+h[2]+h[2];
    var n = parseInt(h, 16);
    return { r: (n >> 16) & 255, g: (n >> 8) & 255, b: n & 255 };
  }

  function fmt3(n) { return String(n).padStart(3, '0'); }

  function draw() {
    if (!ctx || !loaded) return;

    // Base fill (dark)
    ctx.fillStyle = '#060619';
    ctx.fillRect(0, 0, W, H);

    // Per-dir: colored bar proportional to fileCount
    for (var r = 0; r < ROWS; r++) {
      var bid = dirBox[r];
      if (!bid) continue;
      var box = boxById[bid];
      if (!box) continue;
      var rgb = hexToRgb(box.color);
      var y = r * CELL;
      var fc = dirFileCount[r];

      // Fill bar: width proportional to fileCount (max COLS)
      var barW = fc > 0 ? Math.min(fc, COLS) * CELL : 0;
      if (barW > 0) {
        ctx.fillStyle = 'rgba(' + rgb.r + ',' + rgb.g + ',' + rgb.b + ',0.35)';
        ctx.fillRect(0, y, barW, CELL);
      }

      // Category line: 1px horizontal, same width as bar
      if (barW > 0) {
        ctx.fillStyle = 'rgba(' + rgb.r + ',' + rgb.g + ',' + rgb.b + ',0.95)';
        ctx.fillRect(0, y + 1, barW, 1);
      }
    }

    // Vertical gridlines
    ctx.fillStyle = 'rgba(255,255,255,0.035)';
    for (var c = 1; c < COLS; c++) {
      ctx.fillRect(c * CELL, 0, 1, H);
    }

    // Row separators (black; white for selected row edges)
    for (var r = 0; r <= ROWS; r++) {
      var isSelEdge = (r === selRow) || (r === selRow + 1);
      ctx.fillStyle = isSelEdge ? '#ffffff' : '#000000';
      ctx.fillRect(0, r * CELL, W, 1);
    }

    // Selected column highlight
    ctx.fillStyle = 'rgba(255,225,64,0.55)';
    ctx.fillRect(selCol * CELL, 0, CELL, H);

    // Intersection cell: white
    ctx.fillStyle = '#ffffff';
    ctx.fillRect(selCol * CELL, selRow * CELL, CELL, CELL);
  }

  function updateHeader() {
    if (dirVal) dirVal.textContent = fmt3(selRow);
    if (fileVal) fileVal.textContent = fmt3(selCol);
    var bid = (dirBox && selRow < ROWS) ? dirBox[selRow] : 0;
    var box = boxById[bid];
    if (catlabel) catlabel.textContent = box ? box.name : '-';
    if (catpill) catpill.style.background = box ? box.color : '#444';
  }

  function keepRowVisible(row) {
    if (!wrap) return;
    var y = row * CELL;
    var viewTop = wrap.scrollTop;
    var viewH = wrap.clientHeight;
    var target = Math.max(0, y - Math.floor(viewH / 2));
    if (y < viewTop + CELL || y > viewTop + viewH - CELL) {
      wrap.scrollTop = target;
    }
  }

  function setSelection(row, col, scrollRow) {
    selRow = Math.max(0, Math.min(ROWS - 1, row | 0));
    selCol = Math.max(0, Math.min(COLS - 1, col | 0));
    if (dirSlider) dirSlider.value = selRow;
    if (fileSlider) fileSlider.value = selCol;
    updateHeader();
    if (scrollRow) keepRowVisible(selRow);
    draw();
  }

  function bindStepper(btn, deltaRow, deltaCol) {
    if (!btn) return;
    var timer = null, isDown = false;
    var step = function() { setSelection(selRow + deltaRow, selCol + deltaCol, !!deltaRow); };

    btn.addEventListener('pointerdown', function(e) {
      e.preventDefault();
      if (isDown) return;
      isDown = true;
      step();
      timer = setInterval(step, 130);
    });
    var stop = function() { isDown = false; if (timer) { clearInterval(timer); timer = null; } };
    btn.addEventListener('pointerup', stop);
    btn.addEventListener('pointercancel', stop);
    btn.addEventListener('pointerleave', stop);
    btn.addEventListener('click', function(e) { e.preventDefault(); });
  }

  function init() {
    canvas = document.getElementById('mg-grid');
    wrap = document.getElementById('mg-wrap');
    dirSlider = document.getElementById('mg-dir');
    fileSlider = document.getElementById('mg-file');
    dirVal = document.getElementById('mg-dir-val');
    fileVal = document.getElementById('mg-file-val');
    catpill = document.getElementById('mg-catpill');
    catlabel = document.getElementById('mg-catlabel');
    closeBtn = document.getElementById('mg-close');

    if (!canvas || !wrap) return;

    // HiDPI crisp canvas
    var dpr = Math.max(1, Math.floor(window.devicePixelRatio || 1));
    canvas.style.width = W + 'px';
    canvas.style.height = H + 'px';
    canvas.width = W * dpr;
    canvas.height = H * dpr;
    ctx = canvas.getContext('2d', { alpha: false });
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    // Slider events
    if (dirSlider) dirSlider.oninput = function() {
      setSelection(parseInt(dirSlider.value, 10) || 0, selCol, true);
    };
    if (fileSlider) fileSlider.oninput = function() {
      setSelection(selRow, parseInt(fileSlider.value, 10) || 0, false);
    };

    // Canvas tap
    canvas.addEventListener('pointerdown', function(e) {
      var rect = canvas.getBoundingClientRect();
      var x = Math.max(0, Math.min(W - 1, Math.floor(e.clientX - rect.left)));
      var y = Math.max(0, Math.min(H - 1, Math.floor(e.clientY - rect.top)));
      setSelection(Math.floor(y / CELL), Math.floor(x / CELL), true);
    });

    // Canvas double-tap: play file if tapped on file column, set dir theme if tapped on dir label
    canvas.addEventListener('dblclick', function(e) {
      e.preventDefault();
      if (selRow > 0 && selCol > 0) {
        fetch('/api/audio/play?dir=' + selRow + '&file=' + selCol).catch(function() {});
      }
    });

    // Steppers
    bindStepper(document.getElementById('mg-dir-dec'), -1, 0);
    bindStepper(document.getElementById('mg-dir-inc'), 1, 0);
    bindStepper(document.getElementById('mg-file-dec'), 0, -1);
    bindStepper(document.getElementById('mg-file-inc'), 0, 1);

    // DIR label + value click: set single-dir theme_box
    var dirLabel = document.getElementById('mg-dir-label');
    var dirClickTargets = [dirVal, dirLabel];
    dirClickTargets.forEach(function(el) {
      if (!el) return;
      el.style.cursor = 'pointer';
      el.addEventListener('click', function(e) {
        e.preventDefault();
        if (selRow > 0) {
          fetch('/api/audio/themebox?dir=' + selRow).catch(function() {});
          if (dirVal) { dirVal.style.color = '#2ee88a'; setTimeout(function() { dirVal.style.color = ''; }, 400); }
        }
      });
    });

    // FILE label + value click: play that file
    var fileLabel = document.getElementById('mg-file-label');
    var fileClickTargets = [fileVal, fileLabel];
    fileClickTargets.forEach(function(el) {
      if (!el) return;
      el.style.cursor = 'pointer';
      el.addEventListener('click', function(e) {
        e.preventDefault();
        if (selRow > 0 && selCol > 0) {
          fetch('/api/audio/play?dir=' + selRow + '&file=' + selCol).catch(function() {});
          if (fileVal) { fileVal.style.color = '#e8c72e'; setTimeout(function() { fileVal.style.color = ''; }, 400); }
        }
      });
    });
  }

  function load() {
    if (loaded) { draw(); return; }
    fetch('/api/audio/grid')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        boxById = {};
        dirBox = new Uint8Array(ROWS);
        dirFileCount = new Uint16Array(ROWS);

        if (data.boxes) {
          data.boxes.forEach(function(b) {
            boxById[b.id] = { name: b.name, color: b.color };
          });
        }
        if (data.dirs) {
          data.dirs.forEach(function(d) {
            if (d.d < ROWS) {
              dirBox[d.d] = d.b;
              dirFileCount[d.d] = d.n;
            }
          });
        }
        loaded = true;
        draw();
        updateHeader();
      })
      .catch(function(e) { console.error('[mp3grid] load failed:', e); });
  }

  return {
    init: init,
    load: load,
    setSelection: setSelection
  };
})();
