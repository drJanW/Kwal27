/**
 * @file    demo.js
 * @version 260605B
 * @date    2026-06-05
 *
 * Kwal - Demo controls (31-chapter showcase)
 */
(function() {
    const slider = document.getElementById('demoDur');
    const label  = document.getElementById('demoDurLabel');
    const btn    = document.getElementById('btnDemo');
    if (!slider || !label || !btn) return;

    label.textContent = slider.value + ' s';
    slider.addEventListener('input', function() {
        label.textContent = this.value + ' s';
    });

    function setActive(active, chapter) {
        if (active) {
            const c = (typeof chapter === 'number') ? (chapter + 1) : 1;
            btn.textContent = '⏹ Stop';
            btn.title = 'Stop demo (chapter ' + c + ')';
            btn.onclick = stopDemo;
            btn.classList.add('demo-active');
        } else {
            btn.textContent = '🎬 Demo';
            btn.title = 'Demo';
            btn.onclick = startDemo;
            btn.classList.remove('demo-active');
        }
    }

    function startDemo() {
        const seconds = parseInt(slider.value, 10) || parseInt(slider.min, 10) || 300;
        const ms = seconds * 1000;
        if (!confirm('Start demo (' + seconds + ' s)?')) return;
        fetch('/api/demomode?duration=' + ms)
            .then(function(r) { return r.json(); })
            .then(function(d) { if (d.active) setActive(true, 0); });
    }

    function stopDemo() {
        fetch('/api/demostop')
            .then(function(r) { return r.json(); })
            .then(function() { setActive(false); });
    }

    btn.onclick = startDemo;

    if (Kwal.sse && Kwal.sse.onState) {
        Kwal.sse.onState(function(data) {
            if (typeof data.demoActive === 'boolean') {
                setActive(data.demoActive, data.demoChapter);
            }
        });
    }
})();
