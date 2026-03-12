/**
 * @file    sse.js
 * @version 260312A
 * @date    2026-03-12
 *
 * Kwal - Server-Sent Events for live updates
 * Events: state, patterns, colors (legacy: fragment, light)
 */
(function() {
    'use strict';
    
    let eventSource = null;
    let reconnectTimer = null;
    let hasConnectedOnce = false;
    const RECONNECT_POLL_MS = 2000;
    
    // Callbacks registered by other modules
    const listeners = {
        state: [],      // New unified state event
        fragment: [],   // Legacy (still fired by firmware)
        light: [],      // Legacy (still fired by firmware)
        colors: [],
        patterns: [],
        luxcal: [],     // Lux calibration sample captured
        luxcalfit: [],  // Lux calibration fit results (auto or manual)
        reconnect: []   // Called on reconnect to refresh state
    };
    
    function connect() {
        if (eventSource) {
            eventSource.close();
        }
        
        eventSource = new EventSource('/api/events');
        
        eventSource.onopen = function() {
            console.log('[SSE] Connected');
            if (reconnectTimer) {
                clearTimeout(reconnectTimer);
                reconnectTimer = null;
            }
            // On reconnect (not first connect), refresh all data
            // ESP32 may have rebooted with new random pattern/color
            if (hasConnectedOnce) {
                console.log('[SSE] Reconnected - refreshing state');
                listeners.reconnect.forEach(function(cb) { cb(); });
            }
            hasConnectedOnce = true;
        };
        
        eventSource.onerror = function(e) {
            console.warn('[SSE] Error, reconnecting...', e);
            eventSource.close();
            eventSource = null;
            scheduleReconnect();
        };
        
        // Unified state event (new)
        eventSource.addEventListener('state', function(e) {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] state:', data);
                listeners.state.forEach(cb => cb(data));
            } catch (err) {
                console.error('[SSE] state parse error:', err);
            }
        });
        
        // Fragment change event (legacy)
        eventSource.addEventListener('fragment', function(e) {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] fragment:', data);
                listeners.fragment.forEach(cb => cb(data.dir, data.file, data.score));
            } catch (err) {
                console.error('[SSE] fragment parse error:', err);
            }
        });
        
        // Light change event (pattern/color)
        eventSource.addEventListener('light', function(e) {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] light:', data);
                listeners.light.forEach(cb => cb(data.pattern, data.color));
            } catch (err) {
                console.error('[SSE] light parse error:', err);
            }
        });
        
        // Colors list change event
        eventSource.addEventListener('colors', function(e) {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] colors:', data);
                listeners.colors.forEach(cb => cb(data));
            } catch (err) {
                console.error('[SSE] colors parse error:', err);
            }
        });
        
        // Patterns list change event
        eventSource.addEventListener('patterns', function(e) {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] patterns:', data);
                listeners.patterns.forEach(cb => cb(data));
            } catch (err) {
                console.error('[SSE] patterns parse error:', err);
            }
        });

        // Lux calibration sample captured
        eventSource.addEventListener('luxcal', function(e) {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] luxcal:', data);
                listeners.luxcal.forEach(cb => cb(data));
            } catch (err) {
                console.error('[SSE] luxcal parse error:', err);
            }
        });

        // Lux calibration fit results (auto-fit or manual)
        eventSource.addEventListener('luxcalfit', function(e) {
            try {
                const data = JSON.parse(e.data);
                console.log('[SSE] luxcalfit:', data);
                listeners.luxcalfit.forEach(cb => cb(data));
            } catch (err) {
                console.error('[SSE] luxcalfit parse error:', err);
            }
        });
    }
    
    function scheduleReconnect() {
        if (reconnectTimer) return;
        console.log('[SSE] Starting reconnect polling...');
        reconnectTimer = setInterval(async function() {
            try {
                const response = await fetch('/api/health');
                if (response.ok) {
                    console.log('[SSE] Device back online, reloading page...');
                    clearInterval(reconnectTimer);
                    reconnectTimer = null;
                    location.reload();
                }
            } catch (e) {
                // Still offline, keep polling
            }
        }, RECONNECT_POLL_MS);
    }
    
    function disconnect() {
        if (reconnectTimer) {
            clearInterval(reconnectTimer);
            reconnectTimer = null;
        }
        if (eventSource) {
            eventSource.close();
            eventSource = null;
        }
    }
    
    /**
     * Register callback for fragment changes
     * @param {function(dir: number, file: number): void} cb
     */
    function onFragment(cb) {
        if (typeof cb === 'function') {
            listeners.fragment.push(cb);
        }
    }
    
    /**
     * Register callback for light changes
     * @param {function(patternId: string, colorId: string): void} cb
     */
    function onLight(cb) {
        if (typeof cb === 'function') {
            listeners.light.push(cb);
        }
    }
    
    /**
     * Register callback for colors list changes
     * @param {function(data: object): void} cb
     */
    function onColors(cb) {
        if (typeof cb === 'function') {
            listeners.colors.push(cb);
        }
    }
    
    /**
     * Register callback for patterns list changes
     * @param {function(data: object): void} cb
     */
    function onPatterns(cb) {
        if (typeof cb === 'function') {
            listeners.patterns.push(cb);
        }
    }

    /**
     * Register callback for lux calibration sample events
     * @param {function(data: {n: number, lux: number, brightness: number}): void} cb
     */
    function onLuxcal(cb) {
        if (typeof cb === 'function') {
            listeners.luxcal.push(cb);
        }
    }

    /**
     * Register callback for lux calibration fit result events
     * @param {function(data: object): void} cb
     */
    function onLuxcalFit(cb) {
        if (typeof cb === 'function') {
            listeners.luxcalfit.push(cb);
        }
    }
    
    /**
     * Register callback for reconnect (to refresh state after ESP32 reboot)
     * @param {function(): void} cb
     */
    function onReconnect(cb) {
        if (typeof cb === 'function') {
            listeners.reconnect.push(cb);
        }
    }
    
    /**
     * Register callback for unified state event
     * @param {function(data: {brightness, audioLevel, patternId, patternLabel, colorId, colorLabel, fragment}): void} cb
     */
    function onState(cb) {
        if (typeof cb === 'function') {
            listeners.state.push(cb);
        }
    }
    
    // Export
    Kwal.sse = {
        connect: connect,
        disconnect: disconnect,
        onState: onState,
        onFragment: onFragment,
        onLight: onLight,
        onColors: onColors,
        onPatterns: onPatterns,
        onLuxcal: onLuxcal,
        onLuxcalFit: onLuxcalFit,
        onReconnect: onReconnect
    };
})();
