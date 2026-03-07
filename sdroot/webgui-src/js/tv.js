// TV Simulator controls

(function() {
    const slider = document.getElementById('tvHours');
    const label = document.getElementById('tvHoursLabel');
    const btn = document.getElementById('btnTvMode');
    if (!slider || !label || !btn) return;

    slider.addEventListener('input', function() {
        label.textContent = this.value + ' uur';
    });

    var panels = document.querySelectorAll('#app > .panel, #app > .panel-sep, #save-buttons');

    function setActive(active) {
        if (active) {
            btn.textContent = '⏹ Stop TV';
            btn.onclick = stopTvMode;
            btn.classList.add('tv-active');
        } else {
            btn.textContent = '📺 TV';
            btn.onclick = startTvMode;
            btn.classList.remove('tv-active');
        }
        panels.forEach(function(el) {
            if (active) el.classList.add('tv-disabled');
            else el.classList.remove('tv-disabled');
        });
    }

    function startTvMode() {
        var hours = slider.value || 4;
        if (!confirm('Start TV Simulator voor ' + hours + ' uur?')) return;
        fetch('/api/tvmode?hours=' + hours)
            .then(function(r) { return r.json(); })
            .then(function(d) { if (d.active) setActive(true); });
    }

    function stopTvMode() {
        fetch('/api/tvstop')
            .then(function(r) { return r.json(); })
            .then(function() { setActive(false); });
    }

    btn.onclick = startTvMode;
})();
