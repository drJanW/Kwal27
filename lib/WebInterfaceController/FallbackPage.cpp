/**
 * @file FallbackPage.cpp
 * @brief PROGMEM fallback HTML definition — single copy in flash
 * @version 260218E
 * @date 2026-02-18
 */
#include <Arduino.h>
#include "FallbackPage.h"

const char FALLBACK_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kwal – SD Failure</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:16px;max-width:480px;margin:0 auto}
h1{color:#ff6b6b;margin-bottom:8px;font-size:1.4em}
h2{color:#aaa;margin:16px 0 8px;font-size:1.1em}
p{margin:6px 0;line-height:1.4}
.warn{background:#3a1a1a;border:1px solid #ff6b6b;border-radius:8px;padding:12px;margin-bottom:12px}
table{width:100%;border-collapse:collapse;margin:8px 0}
td{padding:4px 8px;border-bottom:1px solid #333}
td:first-child{color:#888;width:40%}
.card{background:#16213e;border-radius:8px;padding:12px;margin:12px 0}
input[type=file]{margin:8px 0;color:#e0e0e0}
button,.btn{background:#0f3460;color:#e0e0e0;border:none;border-radius:6px;padding:8px 16px;cursor:pointer;margin:4px 4px 4px 0;font-size:0.95em}
button:hover,.btn:hover{background:#1a5276}
.ok{color:#6bcb77}.fail{color:#ff6b6b}
#otaSt{margin-top:8px;font-style:italic}
#prog{display:none;width:100%;height:6px;margin-top:6px;appearance:none;border-radius:3px;overflow:hidden}
#prog::-webkit-progress-bar{background:#333}#prog::-webkit-progress-value{background:#6bcb77}
</style>
</head><body>
<div class="warn"><h1>&#9888; SD Card Failure</h1>
<p>The SD card is not available. WebGUI, audio, config files and LED maps are offline.</p></div>
<div class="card"><h2>Device Status</h2><div id="hp">Loading...</div></div>
<div class="card"><h2>OTA Firmware Update</h2>
<form id="ota"><input type="file" name="firmware" accept=".bin">
<button type="submit">Upload Firmware</button></form>
<progress id="prog" max="100" value="0"></progress>
<div id="otaSt"></div></div>
<div class="card"><h2>Actions</h2>
<button onclick="doRestart()">Restart Device</button>
</div>
<script>
function $(id){return document.getElementById(id)}
function loadHealth(){
 fetch('/api/health').then(r=>r.json()).then(d=>{
  let h='<table>';
  h+='<tr><td>Device</td><td>'+d.device+'</td></tr>';
  h+='<tr><td>Firmware</td><td>'+d.firmware+'</td></tr>';
  if(d.ntpDate)h+='<tr><td>Date</td><td>'+d.ntpDate+'</td></tr>';
  if(d.ntpTime)h+='<tr><td>Time</td><td>'+d.ntpTime+'</td></tr>';
  if(d.rtcTempC!=null)h+='<tr><td>Temperature</td><td>'+d.rtcTempC+' &deg;C</td></tr>';
  h+='<tr><td>Heap</td><td>'+d.heapFree+'k free / '+d.heapMin+'k min</td></tr>';
  h+='<tr><td>Timers</td><td>'+d.timers+' / '+d.maxTimers+'</td></tr>';
  h+='<tr><td>SD</td><td class="fail">FAIL</td></tr>';
  h+='</table>';
  $('hp').innerHTML=h;
 }).catch(()=>{$('hp').textContent='Health check failed';});
}
loadHealth();setInterval(loadHealth,30000);

$('ota').onsubmit=function(e){
 e.preventDefault();
 var f=this.firmware.files[0];if(!f)return;
 var fd=new FormData();fd.append('firmware',f);
 var st=$('otaSt'),pg=$('prog');
 st.textContent='Uploading...';pg.style.display='block';pg.value=0;
 var xhr=new XMLHttpRequest();
 xhr.upload.onprogress=function(ev){if(ev.lengthComputable)pg.value=Math.round(ev.loaded/ev.total*100);};
 xhr.onload=function(){
  try{var d=JSON.parse(xhr.responseText);
   if(d.status==='ok'){st.textContent='Success! Rebooting in 2s...';pg.value=100;}
   else st.textContent='Error: '+(d.error||'Unknown');
  }catch(x){st.textContent='Error: '+xhr.responseText;}
 };
 xhr.onerror=function(){st.textContent='Upload failed (network error)';};
 xhr.open('POST','/ota/upload');xhr.send(fd);
};

function doRestart(){
 fetch('/api/restart',{method:'POST'}).then(()=>{
  $('otaSt').textContent='Restarting...';
 }).catch(()=>{$('otaSt').textContent='Restart request failed';});
}
</script>
</body></html>)rawhtml";
