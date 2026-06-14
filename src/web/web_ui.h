#pragma once

const char WEB_INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>SCR Terminal</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#000;color:#00e5ff;font-family:'Courier New',monospace;font-size:14px;padding:8px}
h1{color:#00e5ff;font-size:20px;text-align:center;border-bottom:1px solid #007280;padding:8px 0;margin-bottom:10px}
.tabs{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:10px}
.tab{background:#000;color:#007280;border:1px solid #007280;padding:8px 12px;cursor:pointer;flex:1;text-align:center;font-family:inherit;font-size:13px}
.tab.active{background:#007280;color:#00e5ff;border-color:#00e5ff}
.panel{display:none;border:1px solid #007280;padding:10px}
.panel.active{display:block}
.row{display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid #003840}
.row:last-child{border:none}
.lbl{color:#007280}
.val{color:#00e5ff;text-align:right}
input,textarea,select{background:#000;color:#00e5ff;border:1px solid #007280;padding:6px;width:100%;font-family:inherit;font-size:14px;margin:4px 0}
input:focus,textarea:focus{border-color:#00e5ff;outline:none}
button,.btn{background:#000;color:#00e5ff;border:1px solid #00e5ff;padding:10px;width:100%;cursor:pointer;font-family:inherit;font-size:14px;margin:4px 0}
button:active,.btn:active{background:#007280}
.btn-danger{border-color:#f33;color:#f33}
.btn-warn{border-color:#f90;color:#f90}
.msg{background:#001a1f;border:1px solid #003840;padding:6px;margin:3px 0;font-size:12px}
.msg .from{color:#00e5ff;font-weight:bold}
.msg .txt{color:#00e5ff}
.tag{background:#001a1f;border:1px solid #003840;padding:6px;margin:3px 0;display:flex;justify-content:space-between;align-items:center}
.tag .uid{color:#00e5ff;font-size:12px}
.badge{display:inline-block;background:#007280;color:#00e5ff;padding:2px 6px;font-size:11px;margin-left:6px}
#log{background:#001a1f;border:1px solid #003840;padding:6px;height:120px;overflow-y:auto;font-size:11px;color:#007280;margin:6px 0}
.slider-row{display:flex;align-items:center;gap:8px}
.slider-row input[type=range]{flex:1}
.switch-row{display:flex;justify-content:space-between;align-items:center;padding:6px 0}
.toggle{position:relative;width:44px;height:22px;cursor:pointer}
.toggle input{opacity:0;width:0;height:0}
.toggle .slider{position:absolute;top:0;left:0;right:0;bottom:0;background:#003840;border:1px solid #007280}
.toggle input:checked+.slider{background:#007280;border-color:#00e5ff}
.toggle .slider:before{content:'';position:absolute;width:16px;height:16px;left:2px;top:2px;background:#007280;transition:.2s}
.toggle input:checked+.slider:before{transform:translateX(22px);background:#00e5ff}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.status-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px}
.on{background:#00e5ff}
.off{background:#333}
#wifi-results{max-height:200px;overflow-y:auto}
.modal{display:none;position:fixed;z-index:100;left:0;top:0;width:100%;height:100%;background-color:rgba(0,0,0,0.85);overflow:auto}
.modal-content{background:#000;margin:15% auto;padding:15px;border:1px solid #00e5ff;width:85%;max-width:380px}
</style>
</head>
<body>
<h1>[ SCR TERMINAL ]</h1>

<div class="tabs">
<button class="tab active" onclick="showTab('dash')">DASH</button>
<button class="tab" onclick="showTab('nfc')">NFC</button>
<button class="tab" onclick="showTab('lora')">LORA</button>
<button class="tab" onclick="showTab('wifi')">WiFi</button>
<button class="tab" onclick="showTab('recon')">RECON</button>
<button class="tab" onclick="showTab('hid')">HID</button>
<button class="tab" onclick="showTab('settings')">SET</button>
</div>

<!-- DASHBOARD -->
<div id="dash" class="panel active">
<div class="row"><span class="lbl">TIME</span><span class="val" id="d-time">--:--:--</span></div>
<div class="row"><span class="lbl">DATE</span><span class="val" id="d-date">----</span></div>
<div class="row"><span class="lbl">BATTERY</span><span class="val" id="d-bat">--%</span></div>
<div class="row"><span class="lbl">GPS</span><span class="val" id="d-gps">NO FIX</span></div>
<div class="row"><span class="lbl">NTP</span><span class="val" id="d-ntp">--</span></div>
<div class="row"><span class="lbl">HEAP</span><span class="val" id="d-heap">--</span></div>
<div class="row"><span class="lbl">UPTIME</span><span class="val" id="d-up">--</span></div>
<div class="grid2" style="margin-top:8px">
<button onclick="api('/api/haptic','POST')">HAPTIC TEST</button>
<button onclick="api('/api/brightness','POST',{v:255})">MAX BRIGHT</button>
</div>
</div>

<!-- NFC -->
<div id="nfc" class="panel">
<button onclick="nfcScan()">SCAN TAG</button>
<div class="row"><span class="lbl">STATUS</span><span class="val" id="nfc-status">--</span></div>
<div class="row"><span class="lbl">LAST UID</span><span class="val" id="nfc-uid">--</span></div>
<div class="row"><span class="lbl">NDEF</span><span class="val" id="nfc-ndef">--</span></div>
<button onclick="nfcSave()" style="margin-top:6px">SAVE TAG</button>
<h3 style="color:#007280;margin:8px 0 4px">SAVED TAGS</h3>
<div id="nfc-tags"></div>
<div class="grid2">
<button onclick="nfcExport()">EXPORT ALL (.nfc)</button>
<button onclick="nfcStop()" class="btn-warn">STOP</button>
</div>
</div>

<!-- LORA -->
<div id="lora" class="panel">
<div class="row"><span class="lbl">MESHCORE</span><span class="val" id="lora-status">OFF</span></div>
<div class="grid2">
<button onclick="api('/api/lora/start','POST');el('lora-status','ON')">START RX</button>
<button onclick="api('/api/lora/stop','POST');el('lora-status','OFF')" class="btn-warn">STOP</button>
</div>
<h3 style="color:#007280;margin:8px 0 4px">MESSAGES (public channel)</h3>
<div id="lora-msgs" style="max-height:250px;overflow-y:auto"></div>
<textarea id="lora-txt" rows="2" placeholder="Type message..."></textarea>
<div class="grid2">
<button onclick="loraSend()">SEND</button>
<button onclick="api('/api/lora/advert','POST')">ADVERTISE</button>
</div>
</div>

<!-- WIFI -->
<div id="wifi" class="panel">
<button onclick="wifiScan()">SCAN NETWORKS</button>
<div id="wifi-results"></div>
<h3 style="color:#007280;margin:8px 0 4px">NTP SYNC</h3>
<button onclick="api('/api/ntp','POST')">FORCE NTP SYNC</button>
</div>

<!-- RECON -->
<div id="recon" class="panel">
<div class="grid2">
<button onclick="api('/api/recon/wifi','POST')">SCAN WiFi</button>
<button onclick="api('/api/recon/ble','POST',{duration:10})">SCAN BLE</button>
</div>
<button onclick="api('/api/recon/stop','POST')" class="btn-warn" style="margin-top:4px">STOP ALL</button>
<div id="recon-status" style="color:#007280;margin:6px 0">Ready</div>
<h3 style="color:#007280;margin:6px 0 4px">WiFi NETWORKS (tap to select target)</h3>
<div id="recon-wifi" style="max-height:150px;overflow-y:auto;font-size:11px"></div>
<h3 style="color:#007280;margin:6px 0 4px">BLE DEVICES</h3>
<div id="recon-ble" style="max-height:150px;overflow-y:auto;font-size:11px"></div>
<div class="grid2" style="margin-top:6px">
<button onclick="api('/api/recon/blackout','POST')" class="btn-danger">BLACKOUT</button>
<button onclick="api('/api/recon/sniffer','POST')">SNIFFER</button>
<button onclick="api('/api/recon/detect','POST')">DEAUTH DETECT</button>
<button onclick="startEvilTwin()">EVIL TWIN</button>
</div>
<input id="et-ssid" placeholder="Evil Twin SSID (e.g. FreeWiFi)" style="margin-top:4px">
<h3 style="color:#007280;margin:8px 0 4px">DEAUTH TARGET</h3>
<div class="row"><span class="lbl">BSSID</span><input id="deauth-bssid" placeholder="auto-filled from list" style="width:200px"></div>
<div class="row"><span class="lbl">CH</span><input id="deauth-ch" placeholder="CH" type="number" style="width:80px"></div>
<button onclick="startDeauth()" class="btn-danger">SEND DEAUTH</button>
</div>

<!-- HID -->
<div id="hid" class="panel">
<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px">
<h3 style="color:#007280;margin:0">HID CONTROLLER</h3>
<div style="display:flex;gap:6px">
<button onclick="showSDModal()" style="width:auto;padding:4px 8px;font-size:12px;margin:0">📁 LOAD FROM SD</button>
<button id="btn-hid-kb" onclick="showLayoutModal()" style="width:auto;padding:4px 8px;font-size:12px;margin:0">⌨️ KB: US</button>
</div>
</div>
<div class="row"><span class="lbl">STATUS</span><span class="val" id="hid-status">DISCONNECTED</span></div>
<div class="row"><span class="lbl">SCRIPT</span><span class="val" id="hid-script-status">IDLE</span></div>
<div class="grid2" style="margin-top:6px;margin-bottom:8px">
<button onclick="api('/api/cmd','POST',{cmd:JSON.stringify({cmd:'hid_start'})})">START BLE HID</button>
<button onclick="api('/api/cmd','POST',{cmd:JSON.stringify({cmd:'hid_stop'})})" class="btn-warn">STOP BLE HID</button>
</div>
<div style="position:relative;margin-top:8px">
<textarea id="hid-script-box" rows="10" placeholder="Type DuckyScript here..." style="font-family:monospace;white-space:pre;overflow:auto;resize:both;height:180px"></textarea>
<div style="display:flex;gap:6px;margin-top:4px">
<button onclick="saveScriptToSD()" class="btn-warn" style="flex:1;margin:0">SAVE TO SD</button>
<button onclick="runInstantScript(false)" style="flex:1;margin:0">SEND USB</button>
<button onclick="runInstantScript(true)" style="flex:1;margin:0">SEND BLE</button>
</div>
</div>
<div style="margin-top:8px">
<div style="color:#007280;font-size:12px;margin-bottom:2px;display:flex;justify-content:space-between">
<span>TERMINAL COMMAND LINE</span>
<span>Press Enter to add</span>
</div>
<input id="hid-term-input" placeholder="e.g. STRING hello world..." onkeydown="handleTermEnter(event)">
</div>
<button onclick="api('/api/cmd','POST',{cmd:JSON.stringify({cmd:'hid_abort_script'})})" class="btn-danger" style="margin-top:10px">ABORT RUNNING SCRIPT</button>
</div>

<!-- SETTINGS -->
<div id="settings" class="panel">
<div class="switch-row"><span>WiFi AP</span><span class="val" style="color:#00e5ff">ON (connected)</span></div>
<div class="switch-row"><span>GPS</span><label class="toggle"><input type="checkbox" id="sw-gps" onchange="toggleSvc('gps',this.checked)"><span class="slider"></span></label></div>
<div class="switch-row"><span>NFC Power</span><label class="toggle"><input type="checkbox" id="sw-nfc" onchange="toggleSvc('nfc',this.checked)"><span class="slider"></span></label></div>
<div class="switch-row"><span>Haptic</span><label class="toggle"><input type="checkbox" id="sw-haptic" onchange="toggleSvc('haptic',this.checked)" checked><span class="slider"></span></label></div>
<div class="slider-row" style="margin-top:8px"><span class="lbl">BRIGHTNESS</span><input type="range" min="10" max="255" value="128" oninput="api('/api/brightness','POST',{v:this.value})"></div>
<h3 style="color:#007280;margin:8px 0 4px">WATCHFACE</h3>
<div class="grid2">
<button onclick="api('/api/watchface','POST',{style:'next'})">NEXT FACE</button>
<button onclick="api('/api/watchface','POST',{style:'prev'})">PREV FACE</button>
</div>
<h3 style="color:#007280;margin:8px 0 4px">SYSTEM</h3>
<button onclick="api('/api/reboot','POST')" class="btn-danger">REBOOT WATCH</button>
</div>

<div id="log"></div>

<!-- Modals -->
<div id="layout-modal" class="modal">
<div class="modal-content">
<h3 style="color:#00e5ff;margin-bottom:10px;border-bottom:1px solid #007280;padding-bottom:4px">SELECT KEYBOARD LAYOUT</h3>
<div style="display:grid;grid-template-columns:1fr 1fr 1fr;gap:6px;max-height:200px;overflow-y:auto">
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('US')">US</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('TR')">TR</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('DE')">DE</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('FR')">FR</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('DK')">DK</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('UK')">UK</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('HU')">HU</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('IT')">IT</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('BR')">BR</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('PT')">PT</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('SI')">SI</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('ES')">ES</button>
<button class="btn" style="padding:6px;margin:0" onclick="selectLayout('SV')">SV</button>
</div>
<button onclick="closeLayoutModal()" class="btn-warn" style="margin-top:10px;padding:6px">CANCEL</button>
</div>
</div>

<div id="sd-modal" class="modal">
<div class="modal-content">
<h3 style="color:#00e5ff;margin-bottom:10px;border-bottom:1px solid #007280;padding-bottom:4px">LOAD FROM SD</h3>
<div id="sd-modal-list" style="display:flex;flex-direction:column;gap:4px;max-height:220px;overflow-y:auto"></div>
<button onclick="closeSDModal()" class="btn-warn" style="margin-top:10px;padding:6px">CANCEL</button>
</div>
</div>

<script>
let ws;
function wsConnect(){
 try{
  ws=new WebSocket('ws://'+location.host+'/ws');
  ws.onmessage=e=>{
   try{let d=JSON.parse(e.data);handleWS(d)}catch(x){}
  };
  ws.onclose=()=>setTimeout(wsConnect,3000);
  ws.onerror=()=>{try{ws.close()}catch(x){}};
 }catch(x){setTimeout(wsConnect,3000)}
}
function handleWS(d){
 if(d.type==='status'){
  el('d-time',d.time||'--');el('d-date',d.date||'--');
  el('d-bat',(d.bat||0)+'%');el('d-gps',d.gps||'NO FIX');
  el('d-ntp',d.ntp?'SYNCED':'--');el('d-heap',(d.heap||0)+' KB');
  el('d-up',d.uptime||'--');
  if(d.lora!==undefined) el('lora-status',d.lora?'ON':'OFF');
  if(d.gps!==undefined){let g=document.getElementById('sw-gps');if(g)g.checked=(d.gps!=='OFF')}
  if(d.nfc!==undefined){let n=document.getElementById('sw-nfc');if(n)n.checked=d.nfc}
  if(d.hid_active!==undefined){
   let status='DISCONNECTED';
   if(d.hid_usb)status='USB CONNECTED';
   else if(d.hid_conn)status='BLE CONNECTED';
   else if(d.hid_active)status='BLE ADVERTISING';
   el('hid-status',status);
   el('hid-script-status',d.hid_running?'RUNNING':'IDLE');
   let k=document.getElementById('btn-hid-kb');
   if(k&&d.hid_layout)k.textContent='⌨️ KB: '+d.hid_layout;
  }
 }
 if(d.type==='nfc_tag'){
  el('nfc-uid',d.uid||'--');el('nfc-ndef',d.ndef||'--');
  el('nfc-status','TAG FOUND!');
 }
 if(d.type==='nfc_tags') renderTags(d.tags||[]);
 if(d.type==='lora_msg'){
  let c=document.getElementById('lora-msgs');
  let t=d.ts?new Date(d.ts*1000).toLocaleTimeString():'';
  let meta='';
  if(d.hops!==undefined) meta+=' '+d.hops+'hop';
  if(d.rssi) meta+=' '+d.rssi+'dBm';
  c.innerHTML+='<div class="msg"><span class="from">['+t+meta+']</span> <span class="txt">'+d.text+'</span></div>';
  c.scrollTop=c.scrollHeight;
 }
 if(d.type==='wifi_scan') renderWifi(d.networks||[]);
 if(d.type==='log') log(d.msg);
}
function el(id,v){let e=document.getElementById(id);if(e)e.textContent=v}
function log(m){let l=document.getElementById('log');l.innerHTML+='<div>'+m+'</div>';l.scrollTop=l.scrollHeight}
function showTab(t){
 document.querySelectorAll('.tab').forEach(b=>b.classList.remove('active'));
 document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
 document.getElementById(t).classList.add('active');
 document.querySelectorAll('.tab').forEach(b=>{if(b.textContent.toLowerCase().includes(t.substring(0,3)))b.classList.add('active')});
 if(t==='lora') loadLoraHistory();
}
let loraHistLoaded=false;
async function loadLoraHistory(){
 if(loraHistLoaded) return;
 loraHistLoaded=true;
 let c=document.getElementById('lora-msgs');
 try{
  let r=await fetch('/api/lora/history');
  let t=await r.text();
  let d=JSON.parse(t);
  if(d.messages&&d.messages.length>0){
   // Prepend history before any WS messages
   let old=c.innerHTML;
   c.innerHTML='';
   d.messages.reverse().forEach(m=>{
    let ts=m.ts?new Date(m.ts*1000).toLocaleTimeString():'';
    let meta='';
    if(m.hops!==undefined) meta+=' '+m.hops+'hop';
    if(m.rssi) meta+=' '+m.rssi+'dBm';
    c.innerHTML+='<div class="msg"><span class="from">['+ts+meta+']</span> <span class="txt">'+m.text+'</span></div>';
   });
   c.innerHTML+=old;
   c.scrollTop=c.scrollHeight;
  }
 }catch(e){}
}
async function api(url,method,body){
 try{
  let opts={method:method||'GET'};
  if(body){
   let fd=new URLSearchParams();
   for(let k in body) fd.append(k,body[k]);
   opts.headers={'Content-Type':'application/x-www-form-urlencoded'};
   opts.body=fd.toString();
  }
  let r=await fetch(url,opts);
  let t=await r.text();
  try{let j=JSON.parse(t);if(j.msg)log(j.msg);return j}catch(x){return{}}
 }catch(e){return{}}
}
function nfcScan(){api('/api/nfc/scan','POST');el('nfc-status','SCANNING...')}
function nfcStop(){api('/api/nfc/stop','POST');el('nfc-status','STOPPED')}
function nfcSave(){api('/api/nfc/save','POST')}
function nfcExport(){api('/api/nfc/export','POST')}
function renderTags(tags){
 let c=document.getElementById('nfc-tags');c.innerHTML='';
 tags.forEach((t,i)=>{
  c.innerHTML+='<div class="tag"><span class="uid">'+t.name+'</span><button onclick="api(\'/api/nfc/delete\',\'POST\',{i:'+i+'})" style="width:auto;padding:4px 8px;font-size:11px" class="btn-danger">DEL</button></div>';
 });
}
async function loraSend(){
 let t=document.getElementById('lora-txt');
 if(!t.value)return;
 let r=await api('/api/lora/send','POST',{text:t.value});
 t.value='';

}
function wifiScan(){log('Scanning...');api('/api/wifi/scan','POST')}
function startEvilTwin(){
 let s=document.getElementById('et-ssid').value||'FreeWiFi';
 api('/api/recon/evil_twin','POST',{ssid:s,ch:6});
}
function startDeauth(){
 let b=document.getElementById('deauth-bssid').value;
 let c=document.getElementById('deauth-ch').value;
 if(b&&c) api('/api/recon/deauth','POST',{bssid:b,ch:c});
}

setInterval(async()=>{
 let p=document.getElementById('recon');
 if(!p||!p.classList.contains('active'))return;
 try{
  let r=await fetch('/api/recon/results');
  let d=await r.json();
  let wc=document.getElementById('recon-wifi');
  if(d.wifi&&d.wifi.length>0){
   wc.innerHTML='';
   d.wifi.forEach((n,i)=>{
    wc.innerHTML+='<div class="row" style="cursor:pointer" onclick="document.getElementById(\'deauth-bssid\').value=\''+n.bssid+'\';document.getElementById(\'deauth-ch\').value='+n.ch+'"><span class="lbl">'+n.ssid+'</span><span class="val">['+n.rssi+'] CH'+n.ch+' '+n.auth+'</span></div>';
   });
  }
  let bc=document.getElementById('recon-ble');
  if(d.ble&&d.ble.length>0){
   bc.innerHTML='';
   d.ble.forEach(b=>{
    let tag=b.airtag?' <span style="color:#f90">AIRTAG</span>':'';
    bc.innerHTML+='<div class="row"><span class="lbl">'+(b.name||b.mac)+'</span><span class="val">['+b.rssi+']'+tag+'</span></div>';
   });
  }
 }catch(e){}
},2000);
function renderWifi(nets){
 let c=document.getElementById('wifi-results');c.innerHTML='';
 nets.forEach(n=>{
  c.innerHTML+='<div class="row"><span class="lbl">'+n.ssid+'</span><span class="val">['+n.rssi+'] CH'+n.ch+'</span></div>';
 });
}
function toggleSvc(svc,on){api('/api/service','POST',{service:svc,enable:on})}
function showLayoutModal(){document.getElementById('layout-modal').style.display='block'}
function closeLayoutModal(){document.getElementById('layout-modal').style.display='none'}
function selectLayout(l){
 api('/api/cmd','POST',{cmd:JSON.stringify({cmd:'hid_set_layout',params:{layout:l}})});
 closeLayoutModal();
}
function closeSDModal(){document.getElementById('sd-modal').style.display='none'}
async function showSDModal(){
 let r=await api('/api/cmd','POST',{cmd:JSON.stringify({cmd:'hid_list_scripts'})});
 let list=document.getElementById('sd-modal-list');
 list.innerHTML='';
 if(r&&r.scripts&&r.scripts.length>0){
  r.scripts.forEach(s=>{
   list.innerHTML+='<div class="tag" style="margin:2px 0;"><span style="color:#00e5ff;font-size:13px;cursor:pointer;" onclick="loadScriptContent(\''+s+'\')">'+s+'</span></div>';
  });
 }else{
  list.innerHTML='<div style="color:#007280;padding:10px;text-align:center;">No scripts found in /badusb</div>';
 }
 document.getElementById('sd-modal').style.display='block';
}
async function loadScriptContent(name){
 let r=await api('/api/cmd','POST',{cmd:JSON.stringify({cmd:'hid_read_script',params:{name:name}})});
 if(r&&r.ok){
  document.getElementById('hid-script-box').value=r.content;
  log('Loaded script: '+name);
 }else{
  log('Error loading script: '+(r.error||'unknown'));
 }
 closeSDModal();
}
async function saveScriptToSD(){
 let name=prompt("Enter script filename (e.g. exploit.txt):");
 if(!name)return;
 name=name.trim();
 if(name==='')return;
 let content=document.getElementById('hid-script-box').value;
 let r=await api('/api/cmd','POST',{cmd:JSON.stringify({cmd:'hid_save_script',params:{name:name,content:content}})});
 if(r&&r.ok){
  log('Script saved to SD as: '+name);
 }else{
  log('Error saving script: '+(r.error||'unknown'));
 }
}
async function runInstantScript(ble){
 let content=document.getElementById('hid-script-box').value;
 let layout=document.getElementById('btn-hid-kb').textContent.replace('⌨️ KB: ','');
 let r=await api('/api/cmd','POST',{cmd:JSON.stringify({cmd:'hid_run_instant',params:{script:content,ble:ble,layout:layout}})});
 if(r&&r.ok){
  log('Instant script sent ('+(ble?'BLE':'USB')+')');
 }else{
  log('Error running script: '+(r.error||'unknown'));
 }
}
function handleTermEnter(e){
 if(e.key==='Enter'){
  e.preventDefault();
  let inp=document.getElementById('hid-term-input');
  let box=document.getElementById('hid-script-box');
  if(inp&&box){
   let val=inp.value.trim();
   if(val){
    box.value+=(box.value?"\n":"")+val;
    inp.value='';
    box.scrollTop=box.scrollHeight;
   }
  }
 }
}
wsConnect();
setInterval(()=>{if(ws&&ws.readyState===1)ws.send('ping')},5000);
</script>
</body>
</html>
)rawliteral";
