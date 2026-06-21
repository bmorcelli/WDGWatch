#pragma once

const char WEB_INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>SCR Terminal</title>
<style>
html,body{height:100%;overflow:hidden}
*{box-sizing:border-box;margin:0;padding:0}
body{background:#000;color:#00e5ff;font-family:'Courier New',monospace;font-size:14px;padding:8px;display:flex;flex-direction:column}
h1{color:#00e5ff;font-size:20px;text-align:center;border-bottom:1px solid #007280;padding:8px 0;margin-bottom:10px;flex-shrink:0}
.tabs{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:10px;flex-shrink:0}
.tab{background:#000;color:#007280;border:1px solid #007280;padding:8px 12px;cursor:pointer;flex:1;text-align:center;font-family:inherit;font-size:13px}
.tab.active{background:#007280;color:#00e5ff;border-color:#00e5ff}
.panel{display:none;border:1px solid #007280;padding:10px}
.panel.active{display:flex;flex-direction:column;flex:1;min-height:0}
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

#terminal-output::-webkit-scrollbar {
  width: 8px;
}
#terminal-output::-webkit-scrollbar-track {
  background: #000;
}
#terminal-output::-webkit-scrollbar-thumb {
  background: #007280;
  border-radius: 4px;
}
#terminal-output::-webkit-scrollbar-thumb:hover {
  background: #00e5ff;
}

@keyframes blink-cursor {
  0%, 100% { opacity: 1; }
  50% { opacity: 0; }
}
.blink {
  animation: blink-cursor 1s step-end infinite;
}

#terminal-input {
  background: transparent !important;
  border: none !important;
  color: #00e5ff !important;
  box-shadow: none !important;
  outline: none !important;
  font-family: inherit;
  font-size: inherit;
  width: 100%;
  padding: 0 !important;
  margin: 0 !important;
  caret-color: transparent !important;
}

.modal{display:none;position:fixed;z-index:100;left:0;top:0;width:100%;height:100%;background-color:rgba(0,0,0,0.85);overflow:auto}
.modal-content{background:#000;margin:15% auto;padding:15px;border:1px solid #00e5ff;width:85%;max-width:380px}
.sd-item {
  display:flex;
  justify-content:space-between;
  align-items:center;
  padding:8px;
  border-bottom:1px solid #003840;
}
.sd-item:last-child {
  border:none;
}
.sd-item .name {
  flex:1;
  cursor:pointer;
  color:#00e5ff;
}
.sd-item .name.dir {
  color:#007280;
  font-weight:bold;
}
.sd-item .actions {
  display:flex;
  gap:8px;
}
.sd-btn {
  background:#000;
  color:#00e5ff;
  border:1px solid #007280;
  padding:4px 8px;
  cursor:pointer;
  font-family:inherit;
  font-size:12px;
}
.sd-btn:hover {
  border-color:#00e5ff;
}
.sd-btn-danger {
  border-color:#f33;
  color:#f33;
}
</style>
</head>
<body>
<h1>[ SCR TERMINAL ]</h1>

<div class="tabs">
<button class="tab active" onclick="showTab('cli')">CLI</button>
<button class="tab" onclick="showTab('hid')">HID</button>
<button class="tab" onclick="showTab('sd')">SD FILES</button>
</div>

<!-- CLI TERMINAL PANEL -->
<div id="cli" class="panel active">
  <div id="terminal-container" style="background:#000;border:none;padding:0;flex:1;font-family:'Courier New',monospace;display:flex;flex-direction:column;justify-content:space-between;position:relative;min-height:0;">
    <div id="terminal-output" style="flex:1;overflow-y:auto;white-space:pre-wrap;line-height:1.4;margin-bottom:10px;word-break:break-all;padding-right:5px;"></div>
    <div id="terminal-input-line" style="display:flex;align-items:center;border-top:1px solid #003840;padding-top:8px;flex-shrink:0;">
      <span id="terminal-prompt" style="color:#00e5ff;font-weight:bold;margin-right:8px;white-space:nowrap;">root@scr-terminal # </span>
      <div style="position:relative;flex:1;display:flex;align-items:center;">
        <input id="terminal-input" type="text" style="background:transparent;border:none;color:#00e5ff;font-family:inherit;font-size:inherit;width:100%;padding:0;margin:0;outline:none;" autofocus autocomplete="off" onkeydown="handleTerminalCommand(event);setTimeout(updateCursor,0)" oninput="updateCursor()" onfocus="document.getElementById('terminal-cursor').style.display='inline-block'" onblur="document.getElementById('terminal-cursor').style.display='none'">
        <span id="terminal-cursor" class="blink" style="position:absolute;left:0;top:50%;transform:translateY(-50%);background:#00e5ff;width:8px;height:15px;display:inline-block;pointer-events:none;"></span>
      </div>
    </div>
  </div>
  
  <!-- Simulated Nano Editor -->
  <div id="nano-editor" style="display:none;flex-direction:column;flex:1;background:#000;border:none;padding:0;min-height:0;">
    <div id="nano-header" style="background:#007280;color:#00e5ff;padding:2px 5px;display:flex;justify-content:space-between;font-size:12px;font-weight:bold;flex-shrink:0;">
      <span>NANO Editor v1.0</span>
      <span id="nano-filename">/badusb/exploit.txt</span>
      <span id="nano-status">Unsaved</span>
    </div>
    <textarea id="nano-textarea" style="flex:1;background:#000;color:#00e5ff;border:none;font-family:'Courier New',monospace;font-size:14px;resize:none;outline:none;padding:5px;margin:0;min-height:0;" onkeydown="handleNanoKeys(event)" oninput="document.getElementById('nano-status').textContent='Modified'"></textarea>
    <div id="nano-footer" style="background:#007280;color:#00e5ff;padding:4px 10px;display:flex;gap:30px;font-size:12px;font-weight:bold;flex-shrink:0;">
      <span>Ctrl+S: Save File</span>
      <span>Ctrl+X: Close / Exit</span>
    </div>
  </div>
</div>

<!-- HID PANEL -->
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

<!-- SD FILES PANEL -->
<div id="sd" class="panel">
<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px;border-bottom:1px solid #007280;padding-bottom:6px">
  <div style="font-weight:bold;color:#00e5ff">SD CARD BROWSER</div>
  <div style="display:flex;gap:6px">
    <button onclick="document.getElementById('sd-file-input').click()" style="width:auto;padding:4px 8px;font-size:12px;margin:0">📤 UPLOAD</button>
    <button onclick="createFolder()" style="width:auto;padding:4px 8px;font-size:12px;margin:0">➕ NEW FOLDER</button>
  </div>
</div>
<input type="file" id="sd-file-input" style="display:none" onchange="uploadFile()">
<div id="sd-path" style="color:#007280;margin-bottom:8px;font-weight:bold;word-break:break-all;flex-shrink:0;">/</div>
<div id="sd-file-list" style="border:1px solid #007280;flex:1;overflow-y:auto;background:#000;min-height:200px;"></div>
</div>

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
let currentMenu = 'root';
let currentDir = '/';
let commandHistory = [];
let historyIndex = -1;
let activeEditorFile = '';

function updateCursor() {
  const input = document.getElementById('terminal-input');
  const cursor = document.getElementById('terminal-cursor');
  if (!input || !cursor) return;
  const canvas = updateCursor.canvas || (updateCursor.canvas = document.createElement('canvas'));
  const ctx = canvas.getContext('2d');
  const computed = window.getComputedStyle(input);
  ctx.font = computed.font || "14px 'Courier New', monospace";
  const textWidth = ctx.measureText(input.value).width;
  cursor.style.left = textWidth + 'px';
}

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
  
  if (d.event) {
    if (d.event === 'rf_jammer_stopped') {
      termPrint("\n[System Notification] RF Jammer timeout reached. Broadcast stopped.", 'info');
    }
    else if (d.event === 'scan_done') {
      if (d.type === 'wifi') {
        termPrint(`\n[System Notification] WiFi Recon Scan completed. Found: ${d.wifi} WiFi APs.`, 'success');
        api('/api/cmd', 'POST', {cmd: 'recon_results'}).then(r => {
          if (r) {
            let out = `\n<span style="color:#00ff66;font-weight:bold;">[WiFi APs Discovered]</span>\n`;
            if (r.wifi && r.wifi.length > 0) {
              r.wifi.forEach(n => {
                out += `  SSID: ${n.ssid.padEnd(20)} | BSSID: ${n.bssid} | CH: ${String(n.ch).padEnd(2)} | RSSI: ${n.rssi} dBm (${n.auth})\n`;
              });
            } else {
              out += `  No WiFi networks found.\n`;
            }
            termPrint(out + "\n", 'normal');
          }
        }).catch(err => {});
      } else if (d.type === 'ble') {
        termPrint(`\n[System Notification] BLE Recon Scan completed. Found: ${d.ble} BLE devices.`, 'success');
        api('/api/cmd', 'POST', {cmd: 'recon_results'}).then(r => {
          if (r) {
            let out = `\n<span style="color:#00ff66;font-weight:bold;">[BLE Devices Discovered]</span>\n`;
            if (r.ble && r.ble.length > 0) {
              r.ble.forEach(b => {
                let info = "";
                if (b.airtag) info = " [AIRTAG]";
                else if (b.flipper) info = " [FLIPPER]";
                out += `  MAC: ${b.mac} | Name: ${(b.name || 'N/A').padEnd(20)} | RSSI: ${b.rssi} dBm${info}\n`;
              });
            } else {
              out += `  No BLE devices found.\n`;
            }
            termPrint(out + "\n", 'normal');
          }
        }).catch(err => {});
      } else {
        termPrint(`\n[System Notification] Recon Scan completed. Found: ${d.wifi} WiFi APs, ${d.ble} BLE devices.`, 'success');
        api('/api/cmd', 'POST', {cmd: 'recon_results'}).then(r => {
          if (r) {
            let out = `\n<span style="color:#00ff66;font-weight:bold;">[WiFi APs Discovered]</span>\n`;
            if (r.wifi && r.wifi.length > 0) {
              r.wifi.forEach(n => {
                out += `  SSID: ${n.ssid.padEnd(20)} | BSSID: ${n.bssid} | CH: ${String(n.ch).padEnd(2)} | RSSI: ${n.rssi} dBm (${n.auth})\n`;
              });
            } else {
              out += `  No WiFi networks found.\n`;
            }
            out += `\n<span style="color:#00ff66;font-weight:bold;">[BLE Devices Discovered]</span>\n`;
            if (r.ble && r.ble.length > 0) {
              r.ble.forEach(b => {
                let info = "";
                if (b.airtag) info = " [AIRTAG]";
                else if (b.flipper) info = " [FLIPPER]";
                out += `  MAC: ${b.mac} | Name: ${(b.name || 'N/A').padEnd(20)} | RSSI: ${b.rssi} dBm${info}\n`;
              });
            } else {
              out += `  No BLE devices found.\n`;
            }
            termPrint(out + "\n", 'normal');
          }
        }).catch(err => {});
      }
    }
    else if (d.event === 'deauth_detected') {
      termPrint(`\n[ALERT] WiFi Deauthentication attack detected! Count: ${d.count}`, 'error');
    }
    else if (d.event === 'nfc_tag') {
      termPrint(`\n[NFC Tag Found] UID: ${d.uid} | NDEF: ${d.ndef || 'Empty'}`, 'success');
    }
    else if (d.event === 'lora_msg') {
      termPrint(`\n[LoRa Message] ch: ${d.channel} | text: ${d.text} (hops: ${d.hops}, rssi: ${d.rssi}dBm)`, 'info');
    }
  }
  
  if (d.type === 'lora_msg') {
    let t = d.ts ? new Date(d.ts*1000).toLocaleTimeString() : '';
    let meta = '';
    if(d.hops !== undefined) meta += ' ' + d.hops + 'hop';
    if(d.rssi) meta += ' ' + d.rssi + 'dBm';
    termPrint(`\n[LoRa Message] [${t}${meta}] from:${d.from || 'public'} -> ${d.text}\n`, 'info');
  }
  
  if (d.type === 'nfc_tag') {
    termPrint(`\n[NFC Tag Found] UID: ${d.uid} | NDEF: ${d.ndef || 'Empty'}\n`, 'success');
  }
  
  if (d.type === 'wifi_scan') {
    let out = "\n[WiFi Scan Done] Networks found:\n";
    (d.networks || []).forEach(n => {
      out += `  SSID: ${n.ssid.padEnd(20)} | BSSID: ${n.bssid} | RSSI: ${n.rssi} dBm | CH: ${n.ch}\n`;
    });
    termPrint(out + "\n", 'success');
  }
  
  if(d.type==='log') log(d.msg);
}

function el(id,v){let e=document.getElementById(id);if(e)e.textContent=v}

function log(m){
  termPrint(`[System Log] ${m}\n`, 'muted');
}

function termPrint(text, type='normal') {
  const outputEl = document.getElementById('terminal-output');
  const containerEl = document.getElementById('terminal-container');
  
  let color = '#00e5ff';
  if (type === 'success') color = '#00ff66';
  if (type === 'error') color = '#ff3333';
  if (type === 'info') color = '#ffaa00';
  if (type === 'muted') color = '#007280';
  
  if (text.trim().startsWith('<') && text.trim().endsWith('>')) {
    outputEl.innerHTML += text;
  } else {
    outputEl.innerHTML += `<span style="color:${color};">${text}</span>`;
  }
  outputEl.scrollTop = outputEl.scrollHeight;
}

function escapeHTML(str) {
  return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#039;");
}

function updatePrompt() {
  let prompt = '';
  if (currentMenu === 'root') {
    prompt = 'root@scr-terminal #';
  } else if (currentMenu === 'sd') {
    prompt = `[sd:${currentDir}] root@scr-terminal #`;
  } else {
    prompt = `[${currentMenu}] root@scr-terminal #`;
  }
  document.getElementById('terminal-prompt').textContent = prompt + ' ';
}

function handleTerminalCommand(e) {
  if (e.key === 'Enter') {
    let inputEl = document.getElementById('terminal-input');
    let cmdLine = inputEl.value;
    inputEl.value = '';
    
    if (cmdLine.trim() !== '') {
      commandHistory.push(cmdLine);
      historyIndex = commandHistory.length;
    }
    executeCLI(cmdLine);
  }
  if (e.key === 'ArrowUp') {
    e.preventDefault();
    if (historyIndex > 0) {
      historyIndex--;
      document.getElementById('terminal-input').value = commandHistory[historyIndex];
    }
  }
  if (e.key === 'ArrowDown') {
    e.preventDefault();
    if (historyIndex < commandHistory.length - 1) {
      historyIndex++;
      document.getElementById('terminal-input').value = commandHistory[historyIndex];
    } else {
      historyIndex = commandHistory.length;
      document.getElementById('terminal-input').value = '';
    }
  }
}

function resolvePath(path) {
  if (!path) return currentDir;
  if (path.startsWith('/')) {
    return '/' + path.replace(/\/+/g, '/').substring(1);
  }
  let newPath = currentDir + '/' + path;
  let parts = newPath.split('/');
  let stack = [];
  for (let p of parts) {
    if (p === '' || p === '.') continue;
    if (p === '..') {
      stack.pop();
    } else {
      stack.push(p);
    }
  }
  return '/' + stack.join('/');
}

function getNeofetchHTML(r) {
  let uptimeSec = r.uptime_s || r.uptime_sec || 0;
  let upStr = '';
  if (uptimeSec > 3600) {
    upStr = Math.floor(uptimeSec / 3600) + 'h ' + Math.floor((uptimeSec % 3600) / 60) + 'm';
  } else {
    upStr = Math.floor(uptimeSec / 60) + 'm ' + (uptimeSec % 60) + 's';
  }
  
  let info = `
    <b><span style="color:#00e5ff;font-size:13px;">root@scr-terminal</span></b><br>
    <span style="color:#007280;">----------------</span><br>
    <span style="color:#ffaa00;">OS:</span> WDGWatch v0.1.1<br>
    <span style="color:#ffaa00;">Kernel:</span> PipBoy-3000<br>
    <span style="color:#ffaa00;">Uptime:</span> ${upStr}<br>
    <span style="color:#ffaa00;">Battery:</span> ${r.bat}% (${r.charging ? 'Charging' : 'Discharging'})<br>
    <span style="color:#ffaa00;">Free RAM:</span> ${r.heap || r.heap_kb || 0} KB<br>
    <span style="color:#ffaa00;">SD Card:</span> ${r.sd_size || '32 GB'} (${r.sd_ok !== false ? 'Mounted' : 'Missing'})<br>
    <span style="color:#ffaa00;">WiFi IP:</span> ${r.ip || '192.168.4.1'}
  `;
  
  let logo = `  ____  ____ ____  
 / ___|/ ___|  _ \\ 
 \\___ \\| |   | |_) |
  ___) || |___| _ < 
 |____/ \\____|_| \\_\\`;

  return `
<div style="display:flex;flex-wrap:wrap;align-items:center;gap:15px;margin-bottom:15px;border-bottom:1px dashed #003840;padding-bottom:15px;font-family:inherit;">
  <pre style="color:#007280;font-weight:bold;margin:0;line-height:1.2;font-family:inherit;font-size:12px;letter-spacing: -0.5px;">${logo}</pre>
  <div style="line-height:1.3;font-family:inherit;color:#00e5ff;font-size:11px;min-width:180px;">${info}</div>
</div>`;
}

async function executeCLI(cmdLine) {
  let promptText = document.getElementById('terminal-prompt').textContent;
  termPrint(`\n${promptText}${escapeHTML(cmdLine)}\n`, 'normal');
  
  let line = cmdLine.trim();
  if (!line) return;
  
  let lowerLine = line.toLowerCase();
  if (lowerLine === 'wifi scan' || lowerLine === 'scan wifi' || lowerLine === 'wifi') {
    line = 'recon wifi';
  } else if (lowerLine.startsWith('ble scan') || lowerLine.startsWith('scan ble') || lowerLine.startsWith('ble ') || lowerLine === 'ble') {
    let parts = lowerLine.split(/\s+/);
    let dur = '';
    if (lowerLine.startsWith('ble scan') || lowerLine.startsWith('scan ble')) {
      dur = parts[2] || '';
    } else {
      dur = parts[1] || '';
    }
    line = ('recon ble ' + dur).trim();
  } else if (lowerLine === 'wifi results' || lowerLine === 'ble results' || lowerLine === 'scan results' || lowerLine === 'results') {
    line = 'recon results';
  }
  
  let parts = line.split(/\s+/);
  let cmd = parts[0].toLowerCase();
  let args = parts.slice(1);
  
  let contextCmd = cmd;
  let contextArgs = args;
  
  const globals = ['help', 'clear', 'exit', 'sysinfo', 'status', 'brightness', 'haptic', 'reboot', 'watchface', 'compass', 'sensor'];
  const menus = ['lora', 'nfc', 'sd', 'hid', 'recon', 'rf', 'gps', 'wardriving', 'pet'];
  
  if (currentMenu !== 'root') {
    if (globals.includes(cmd) || menus.includes(cmd)) {
      contextCmd = cmd;
      contextArgs = args;
    } else {
      contextCmd = currentMenu;
      contextArgs = [cmd].concat(args);
    }
  }
  
  if (contextCmd === 'clear') {
    document.getElementById('terminal-output').innerHTML = '';
    return;
  }
  
  if (contextCmd === 'exit') {
    if (currentMenu !== 'root') {
      let oldMenu = currentMenu;
      currentMenu = 'root';
      updatePrompt();
      termPrint(`Returned to root menu from [${oldMenu}].`, 'info');
      if (oldMenu === 'recon') api('/api/recon/stop', 'POST');
      if (oldMenu === 'nfc') api('/api/nfc/stop', 'POST');
      if (oldMenu === 'lora') api('/api/lora/stop', 'POST');
    } else {
      termPrint("Already at root menu.", 'info');
    }
    return;
  }
  
  if (globals.includes(contextCmd)) {
    await handleGlobalCLI(contextCmd, contextArgs);
    return;
  }
  
  if (menus.includes(contextCmd)) {
    if (contextArgs.length === 0) {
      currentMenu = contextCmd;
      updatePrompt();
      termPrint(`Menu [${currentMenu}] active. Type subcommands or 'exit' to return.`, 'success');
      return;
    }
    await handleMenuCLI(contextCmd, contextArgs);
    return;
  }
  
  const fsCmds = ['ls', 'cd', 'mkdir', 'rm', 'mv', 'cp', 'cat', 'write', 'nano', 'upload', 'download'];
  if (fsCmds.includes(contextCmd)) {
    await handleFsCLI(contextCmd, contextArgs);
    return;
  }
  
  await handleFallbackCLI(contextCmd, contextArgs);
}

async function handleGlobalCLI(cmd, args) {
  if (cmd === 'help') {
    let sub = args[0] ? args[0].toLowerCase() : '';
    if (!sub && currentMenu !== 'root') {
      sub = currentMenu;
    }
    
    if (sub === 'lora') {
      let out = `<b>[LoRa Tools Command Guide]</b>\n`;
      out += `  lora start [mode] [freq]   Start LoRa module (mode: 0:Mesh, 1:Meshtastic, 2:POCSAG, 3:Bruce)\n`;
      out += `  lora pocsag &lt;ric&gt; [freq] Start POCSAG pager receiver\n`;
      out += `  lora stop                  Stop LoRa module\n`;
      out += `  lora send &lt;msg&gt;            Send mesh message\n`;
      out += `  lora advert                Broadcast device description advertisement\n`;
      out += `  lora history               Display last 20 received chat messages\n`;
      out += `  lora setric &lt;ric&gt;          Update POCSAG pager RIC code\n`;
      out += `  lora setfreq &lt;mhz&gt;         Set center carrier frequency\n`;
      out += `  lora setname &lt;name&gt;        Set mesh profile nickname\n`;
      termPrint(out, 'normal');
      return;
    }
    
    if (sub === 'nfc') {
      let out = `<b>[NFC Tools Command Guide]</b>\n`;
      out += `  nfc scan                   Start scanning for cards\n`;
      out += `  nfc stop                   Stop scanning/emulating\n`;
      out += `  nfc save                   Save last read card to SD card\n`;
      out += `  nfc list                   List saved card profiles\n`;
      out += `  nfc delete &lt;idx&gt;           Delete card profile from SD\n`;
      out += `  nfc emulate                Emulate selected card profile\n`;
      out += `  nfc select                 Cycle and select next card profile\n`;
      out += `  nfc status                 Show current NFC controller status\n`;
      termPrint(out, 'normal');
      return;
    }
    
    if (sub === 'recon') {
      let out = `<b>[Recon & WiFi Command Guide]</b>\n`;
      out += `  wifi scan                  Scan for wireless access points\n`;
      out += `  ble scan [seconds]         Scan for BLE devices (default 10s)\n`;
      out += `  recon wifi                 Continuously scan/map wireless APs\n`;
      out += `  recon ble [seconds]        Scan BLE devices for a duration (default 10s)\n`;
      out += `  recon stop                 Abort any active scan or jammer service\n`;
      out += `  recon results              Show found APs and BLE devices\n`;
      out += `  deauth &lt;bssid&gt; &lt;ch&gt;        Launch deauth attack on target BSSID & channel\n`;
      out += `  blackout                   Deauth attack all discovered APs\n`;
      out += `  sniffer &lt;ch&gt;               Capture wireless frames on channel (sniffer stop)\n`;
      out += `  deauth detect              Monitor environment for management frame attacks\n`;
      out += `  eviltwin &lt;ssid&gt; [ch]       Launch Evil Twin portal (eviltwin stop)\n`;
      out += `  arp scan / results         Map active hosts in local subnet\n`;
      out += `  ipsniff &lt;ip&gt; / results     Perform network traffic analysis on IP\n`;
      out += `  beaconspam &lt;ssids&gt;         Flood area with fake SSIDs (beaconspam stop)\n`;
      out += `  adsb start &lt;lat&gt; &lt;lon&gt;     Track aircraft within vicinity\n`;
      out += `  adsb status                Get detected aircraft stats\n`;
      termPrint(out, 'normal');
      return;
    }
    
    if (sub === 'rf') {
      let out = `<b>[RF Tools Command Guide]</b>\n`;
      out += `  rf jam &lt;hz&gt; [duration]     Broadcast RF jammer signal (e.g. rf jam 433920000 10)\n`;
      out += `  rf stop                    Stop RF jammer broadcast\n`;
      out += `  rf status                  Read jammer power/frequency status\n`;
      out += `  tesla                      Transmit Tesla charge port open burst\n`;
      termPrint(out, 'normal');
      return;
    }
    
    if (sub === 'hid') {
      let out = `<b>[HID & BadUSB Command Guide]</b>\n`;
      out += `  hid start / stop           Toggle BLE HID keyboard advertisement\n`;
      out += `  hid status                 Read BLE/USB connection layout & execution state\n`;
      out += `  hid layout &lt;lang&gt;          Set keyboard layout language (US, TR, DE, etc.)\n`;
      out += `  hid list                   List ducky scripts in /badusb on SD card\n`;
      out += `  hid run &lt;file&gt; usb/ble     Run script from SD card using USB or BLE\n`;
      out += `  hid runinstant &lt;script&gt;    Send raw keystroke script immediately\n`;
      out += `  hid abort                  Force abort active script run\n`;
      out += `  airmouse start / stop      Enable or disable air mouse gyro tracking\n`;
      out += `  airmouse cal               Calibrate gyroscope sensor offsets\n`;
      out += `  hid media &lt;vol_up/vol_down/screenshot&gt; Send consumer controls\n`;
      out += `  hid click 1/2              Send mouse clicks (1:left, 2:right)\n`;
      out += `  hid scroll &lt;val&gt;           Send scroll wheel motion offsets\n`;
      termPrint(out, 'normal');
      return;
    }
    
    if (sub === 'gps' || sub === 'wardriving') {
      let out = `<b>[GPS & Wardriving Command Guide]</b>\n`;
      out += `  gps on / off               Toggle hardware GPS receiver module power\n`;
      out += `  gps status                 Read receiver coordination details\n`;
      out += `  wardriving start / stop    Toggle trip logs writing to SD card\n`;
      termPrint(out, 'normal');
      return;
    }
    
    if (sub === 'pet') {
      let out = `<b>[Virtual Pet Command Guide]</b>\n`;
      out += `  pet status                 Show current level, health, cleanliness, energy\n`;
      out += `  pet feed                   Feed the pet to restore energy\n`;
      out += `  pet clean                  Clean up poop and mess\n`;
      out += `  pet heal                   Heal and cure diseases\n`;
      termPrint(out, 'normal');
      return;
    }
    
    if (sub === 'sd' || sub === 'fs') {
      let out = `<b>[Filesystem & SD Command Guide]</b>\n`;
      out += `  ls [path]                  List files in directory\n`;
      out += `  cd &lt;path&gt;                  Change working directory\n`;
      out += `  mkdir &lt;path&gt;               Create a new directory\n`;
      out += `  rm &lt;path&gt;                  Delete file or empty directory\n`;
      out += `  mv &lt;old&gt; &lt;new&gt;             Rename or move file\n`;
      out += `  cp &lt;src&gt; &lt;dest&gt;            Copy file\n`;
      out += `  cat &lt;file&gt;                 Read contents of text file\n`;
      out += `  write &lt;file&gt; &lt;text&gt;        Write simple string to file\n`;
      out += `  nano &lt;file&gt;                Launch interactive text editor\n`;
      out += `  upload                     Upload a file from browser\n`;
      out += `  download &lt;file&gt;            Download a file to browser\n`;
      termPrint(out, 'normal');
      return;
    }

    let out = `<b>SCR Terminal Command Guide:</b>\n`;
    out += `<span style="color:#00e5ff;font-weight:bold;">System:</span>\n`;
    out += `  status              Show system status overview (Neofetch)\n`;
    out += `  sysinfo             Show firmware & hardware info\n`;
    out += `  sensor              Read CPU temperature, battery V, uptime\n`;
    out += `  brightness &lt;val&gt;     Set screen brightness (10-255)\n`;
    out += `  haptic              Test haptic vibration\n`;
    out += `  watchface next/prev Cycle watchface design\n`;
    out += `  compass             Read compass direction\n`;
    out += `  reboot              Restart the watch\n`;
    out += `  clear               Clear screen\n`;
    out += `  exit                Exit active context menu\n\n`;
    
    out += `<span style="color:#00e5ff;font-weight:bold;">Sub-menus (type context name to enter or 'help &lt;menu&gt;'):</span>\n`;
    out += `  nfc | lora | sd | hid | recon | rf | gps | pet\n\n`;
    
    out += `<span style="color:#00e5ff;font-weight:bold;">Filesystem:</span>\n`;
    out += `  ls [path] | cd &lt;path&gt; | mkdir &lt;path&gt; | rm &lt;path&gt;\n`;
    out += `  mv &lt;old&gt; &lt;new&gt; | cp &lt;src&gt; &lt;dest&gt; | cat &lt;file&gt;\n`;
    out += `  write &lt;file&gt; &lt;text&gt; | nano &lt;file&gt; | upload | download &lt;file&gt;\n\n`;
    
    out += `<span style="color:#00e5ff;font-weight:bold;">Recon & Radio:</span>\n`;
    out += `  deauth &lt;bssid&gt; &lt;ch&gt; | blackout | sniffer &lt;ch&gt; | deauth detect\n`;
    out += `  eviltwin &lt;ssid&gt; [ch] | arp scan/results | ipsniff &lt;ip&gt; / results\n`;
    out += `  beaconspam &lt;ssid1,ssid2&gt; | adsb start/status | rf jam &lt;hz&gt; [dur] | tesla\n\n`;
    
    out += `<span style="color:#00e5ff;font-weight:bold;">HID & BadUSB:</span>\n`;
    out += `  hid start/stop/status | hid layout &lt;lang&gt; | hid list | hid run &lt;file&gt; usb/ble\n`;
    out += `  hid runinstant &lt;script&gt; | hid abort | airmouse start/stop/cal\n`;
    out += `  hid media &lt;vol_up/vol_down/screenshot&gt; | hid click 1/2 | hid scroll &lt;val&gt;\n\n`;
    
    out += `<span style="color:#00e5ff;font-weight:bold;">Virtual Pet:</span>\n`;
    out += `  pet status | pet feed | pet clean | pet heal\n`;
    
    termPrint(out, 'normal');
    return;
  }
  
  if (cmd === 'status') {
    let r = await api('/api/cmd', 'POST', {cmd: 'status'});
    if (r) {
      termPrint(getNeofetchHTML(r));
    } else {
      termPrint("Error fetching status.", 'error');
    }
    return;
  }
  
  if (cmd === 'sysinfo') {
    let r = await api('/api/cmd', 'POST', {cmd: 'version'});
    if (r) {
      let out = `OS: ${r.version || 'unknown'}\nCodename: ${r.codename || 'unknown'}\nHardware: ${r.hw || 'unknown'}\n`;
      termPrint(out, 'success');
    } else {
      termPrint("Error fetching sysinfo.", 'error');
    }
    return;
  }
  
  if (cmd === 'sensor') {
    let r = await api('/api/cmd', 'POST', {cmd: 'sensor_data'});
    if (r) {
      let out = `Battery Percent: ${r.bat}%\nBattery Voltage: ${r.bat_v} V\nCharging: ${r.charging?'YES':'NO'}\nFree Heap: ${r.heap_kb} KB\nUptime: ${r.uptime_s} s\n`;
      if (r.gps_lat) out += `GPS Fix: ${r.gps_lat}, ${r.gps_lon} (${r.gps_sats} sats)\n`;
      termPrint(out, 'success');
    } else {
      termPrint("Error fetching sensor data.", 'error');
    }
    return;
  }
  
  if (cmd === 'brightness') {
    let v = parseInt(args[0]);
    if (isNaN(v) || v < 10 || v > 255) {
      termPrint("Usage: brightness <10-255>", 'error');
      return;
    }
    await api('/api/brightness', 'POST', {v: v});
    termPrint("Brightness updated.", 'success');
    return;
  }
  
  if (cmd === 'haptic') {
    await api('/api/haptic', 'POST');
    termPrint("Haptic test triggered.", 'success');
    return;
  }
  
  if (cmd === 'watchface') {
    let action = args[0];
    if (action !== 'next' && action !== 'prev') {
      termPrint("Usage: watchface next/prev", 'error');
      return;
    }
    await api('/api/watchface', 'POST', {style: action});
    termPrint(`Watchface set to ${action}.`, 'success');
    return;
  }
  
  if (cmd === 'compass') {
    let r = await api('/api/cmd', 'POST', {cmd: 'compass'});
    if (r) {
      termPrint(`Heading: ${r.heading}°\nPitch: ${r.pitch}°\nRoll: ${r.roll}°\n`, 'success');
    } else {
      termPrint("Error reading compass.", 'error');
    }
    return;
  }
  
  if (cmd === 'reboot') {
    termPrint("Rebooting device...", 'error');
    await api('/api/reboot', 'POST');
    return;
  }
}

async function handleMenuCLI(menu, args) {
  let sub = args[0].toLowerCase();
  let subArgs = args.slice(1);
  
  if (menu === 'nfc') {
    if (sub === 'scan') {
      await api('/api/cmd', 'POST', {cmd: 'nfc_scan'});
      termPrint("NFC Scanner started. Hold card near watch screen.", 'info');
    } else if (sub === 'stop') {
      await api('/api/cmd', 'POST', {cmd: 'nfc_stop'});
      termPrint("NFC Scan stopped.", 'info');
    } else if (sub === 'save') {
      let r = await api('/api/cmd', 'POST', {cmd: 'nfc_save'});
      if (r && r.ok) termPrint("Card saved to SD.", 'success');
      else termPrint("Save failed or no tag read yet.", 'error');
    } else if (sub === 'list') {
      let r = await api('/api/cmd', 'POST', {cmd: 'nfc_list'});
      if (r && r.tags) {
        let out = "Saved NFC Tags:\n";
        r.tags.forEach((t, i) => {
          out += `  [${i}] ${t}\n`;
        });
        termPrint(out, 'success');
      } else {
        termPrint("No tags found or read error.", 'error');
      }
    } else if (sub === 'delete') {
      let idx = parseInt(subArgs[0]);
      if (isNaN(idx)) { termPrint("Usage: nfc delete <index>", 'error'); return; }
      await api('/api/cmd', 'POST', {cmd: 'nfc_delete', params: {idx: idx}});
      termPrint("NFC card deleted.", 'success');
    } else if (sub === 'emulate') {
      await api('/api/cmd', 'POST', {cmd: 'nfc_emulate'});
      termPrint("NFC Emulation toggled.", 'info');
    } else if (sub === 'select') {
      await api('/api/cmd', 'POST', {cmd: 'nfc_select_next'});
      termPrint("Selected next NFC card.", 'info');
    } else if (sub === 'status') {
      let r = await api('/api/cmd', 'POST', {cmd: 'nfc_status'});
      if (r) {
        termPrint(`NFC Status: ${r.status || 'idle'}\nSelected Card: ${r.card || 'none'}\n`, 'success');
      }
    } else {
      termPrint(`Unknown NFC command: ${sub}`, 'error');
    }
  }
  
  else if (menu === 'lora') {
    if (sub === 'start') {
      let mode = parseInt(subArgs[0] || 0);
      let freq = parseFloat(subArgs[1]);
      let params = {mode: mode};
      if (!isNaN(freq)) params.freq = freq;
      await api('/api/cmd', 'POST', {cmd: 'lora_start', params: params});
      termPrint(`LoRa started in mode ${mode}${!isNaN(freq)?' @ '+freq+'MHz':''}.`, 'success');
    } else if (sub === 'pocsag') {
      let ric = parseInt(subArgs[0]);
      let freq = parseFloat(subArgs[1]);
      if (isNaN(ric)) { termPrint("Usage: lora pocsag <ric> [freq]", 'error'); return; }
      let params = {mode: 2, ric: ric};
      if (!isNaN(freq)) params.freq = freq;
      await api('/api/cmd', 'POST', {cmd: 'lora_start', params: params});
      termPrint(`LoRa POCSAG pager active (RIC: ${ric}).`, 'success');
    } else if (sub === 'stop') {
      await api('/api/cmd', 'POST', {cmd: 'lora_stop'});
      termPrint("LoRa stopped.", 'info');
    } else if (sub === 'send') {
      let text = subArgs.join(' ');
      await api('/api/cmd', 'POST', {cmd: 'lora_send', params: {text: text}});
      termPrint(`Sent LoRa message: ${text}`, 'success');
    } else if (sub === 'advert') {
      await api('/api/cmd', 'POST', {cmd: 'lora_advert'});
      termPrint("LoRa advertise sent.", 'success');
    } else if (sub === 'history') {
      let r = await api('/api/cmd', 'POST', {cmd: 'lora_history'});
      if (r && r.messages) {
        let out = "LoRa Chat History:\n";
        r.messages.forEach(m => {
          let t = m.ts ? new Date(m.ts*1000).toLocaleTimeString() : '';
          out += `  [${t}] ${m.text} (${m.rssi}dBm, ${m.hops}hops)\n`;
        });
        termPrint(out, 'success');
      }
    } else if (sub === 'setric') {
      let ric = parseInt(subArgs[0]);
      if (isNaN(ric)) { termPrint("Usage: lora setric <ric>", 'error'); return; }
      await api('/api/cmd', 'POST', {cmd: 'lora_set_ric', params: {ric: ric}});
      termPrint(`RIC updated to ${ric}`, 'success');
    } else if (sub === 'setfreq') {
      let freq = parseFloat(subArgs[0]);
      if (isNaN(freq)) { termPrint("Usage: lora setfreq <mhz>", 'error'); return; }
      await api('/api/cmd', 'POST', {cmd: 'lora_set_freq', params: {freq: freq}});
      termPrint(`Frequency updated to ${freq} MHz`, 'success');
    } else if (sub === 'setname') {
      let name = subArgs[0];
      await api('/api/cmd', 'POST', {cmd: 'lora_set_name', params: {name: name}});
      termPrint(`LoRa name set to ${name}`, 'success');
    } else {
      termPrint(`Unknown LoRa command: ${sub}`, 'error');
    }
  }
  
  else if (menu === 'recon') {
    if (sub === 'wifi') {
      await api('/api/cmd', 'POST', {cmd: 'recon_wifi'});
      termPrint("Continuous WiFi recon active. View results with 'recon results'.", 'info');
    } else if (sub === 'ble') {
      let dur = parseInt(subArgs[0] || 10);
      await api('/api/cmd', 'POST', {cmd: 'recon_ble', params: {duration: dur}});
      termPrint(`BLE recon scanning for ${dur} seconds...`, 'info');
    } else if (sub === 'stop') {
      await api('/api/cmd', 'POST', {cmd: 'recon_stop'});
      termPrint("Recon stopped.", 'info');
    } else if (sub === 'results') {
      let r = await api('/api/cmd', 'POST', {cmd: 'recon_results'});
      if (r) {
        let out = `<span style="color:#00ff66;font-weight:bold;">WiFi APs found:</span>\n`;
        if (r.wifi && r.wifi.length > 0) {
          r.wifi.forEach(n => {
            out += `  SSID: ${n.ssid.padEnd(20)} | BSSID: ${n.bssid} | CH: ${String(n.ch).padEnd(2)} | RSSI: ${n.rssi} dBm (${n.auth})\n`;
          });
        } else {
          out += `  No networks found yet.\n`;
        }
        out += `\n<span style="color:#00ff66;font-weight:bold;">BLE Devices found:</span>\n`;
        if (r.ble && r.ble.length > 0) {
          r.ble.forEach(b => {
            let info = "";
            if (b.airtag) info = " [AIRTAG]";
            else if (b.flipper) info = " [FLIPPER]";
            out += `  MAC: ${b.mac} | Name: ${(b.name || 'N/A').padEnd(20)} | RSSI: ${b.rssi} dBm${info}\n`;
          });
        } else {
          out += `  No BLE devices found yet.\n`;
        }
        termPrint(out, 'normal');
      }
    } else if (sub === 'arp') {
      let act = subArgs[0];
      if (act === 'scan') {
        await api('/api/cmd', 'POST', {cmd: 'recon_arp_start'});
        termPrint("Subnet ARP scan started.", 'info');
      } else if (act === 'results') {
        let r = await api('/api/cmd', 'POST', {cmd: 'recon_arp_results'});
        if (r && r.devices) {
          let out = "ARP Scan Results:\n";
          r.devices.forEach(d => {
            out += `  IP: ${d.ip.padEnd(15)} | MAC: ${d.mac} | Vendor: ${d.vendor}\n`;
          });
          termPrint(out, 'success');
        }
      } else {
        termPrint("Usage: arp scan/results", 'error');
      }
    } else if (sub === 'ipsniff') {
      let act = subArgs[0];
      if (act === 'results') {
        let r = await api('/api/cmd', 'POST', {cmd: 'recon_ip_sniff_results'});
        if (r && r.ips) {
          let out = `IP Sniffing Statistics:\nUnique Connected IPs:\n`;
          r.ips.forEach(ip => {
            out += `  - ${ip}\n`;
          });
          termPrint(out, 'success');
        }
      } else {
        await api('/api/cmd', 'POST', {cmd: 'recon_ip_sniff', params: {ip: act}});
        termPrint(`IP Sniffer target set to ${act}. dynamical PCAP recording started.`, 'success');
      }
    } else {
      termPrint(`Unknown recon command: ${sub}`, 'error');
    }
  }
  
  else if (menu === 'rf') {
    if (sub === 'jam') {
      let freq = parseInt(subArgs[0]);
      let dur = parseInt(subArgs[1] || 0);
      if (isNaN(freq)) { termPrint("Usage: rf jam <freq_hz> [dur_sec]", 'error'); return; }
      await api('/api/cmd', 'POST', {cmd: 'rf_jammer_start', params: {freq: freq, duration: dur}});
      termPrint(`RF Jammer broadcasting @ ${freq} Hz${dur>0?' for '+dur+'s':''}.`, 'danger');
    } else if (sub === 'stop') {
      await api('/api/cmd', 'POST', {cmd: 'rf_jammer_stop'});
      termPrint("RF Jammer stopped.", 'info');
    } else if (sub === 'status') {
      let r = await api('/api/cmd', 'POST', {cmd: 'rf_status'});
      if (r) {
        termPrint(`Jammer: ${r.active?'JAMMING':'IDLE'}\nFreq: ${r.freq} Hz\nTesla Sending: ${r.tesla_sending?'YES':'NO'}\n`, 'success');
      }
    } else {
      termPrint(`Unknown RF command: ${sub}`, 'error');
    }
  }
  
  else if (menu === 'hid') {
    if (sub === 'start') {
      await api('/api/cmd', 'POST', {cmd: 'hid_start'});
      termPrint("BLE HID advertising started.", 'success');
    } else if (sub === 'stop') {
      await api('/api/cmd', 'POST', {cmd: 'hid_stop'});
      termPrint("BLE HID advertising stopped.", 'info');
    } else if (sub === 'status') {
      let r = await api('/api/cmd', 'POST', {cmd: 'hid_status'});
      if (r) {
        let stat = r.usb_connected ? 'USB Connected' : (r.connected ? 'BLE Connected' : (r.active ? 'BLE Advertising' : 'Disconnected'));
        termPrint(`Status: ${stat}\nKeyboard Layout: ${r.layout}\nRunning Script: ${r.running_script?'YES':'NO'}\n`, 'success');
      }
    } else if (sub === 'layout') {
      let lay = subArgs[0];
      await api('/api/cmd', 'POST', {cmd: 'hid_set_layout', params: {layout: lay}});
      termPrint(`Keyboard layout set to ${lay}.`, 'success');
    } else if (sub === 'list') {
      let r = await api('/api/cmd', 'POST', {cmd: 'hid_list_scripts'});
      if (r && r.scripts) {
        let out = "BadUSB Scripts on SD (/badusb):\n";
        r.scripts.forEach(s => {
          out += `  - ${s}\n`;
        });
        termPrint(out, 'success');
      }
    } else if (sub === 'run') {
      let file = subArgs[0];
      let conn = subArgs[1] || 'ble';
      let r = await api('/api/cmd', 'POST', {cmd: 'hid_run_script', params: {path: file, ble: (conn === 'ble')}});
      if (r && r.ok) termPrint(`Running script ${file} via ${conn.toUpperCase()}...`, 'success');
      else termPrint(`Error: ${r.error || 'could not start'}`, 'error');
    } else if (sub === 'runinstant') {
      let script = subArgs.join(' ');
      let r = await api('/api/cmd', 'POST', {cmd: 'hid_run_instant', params: {script: script}});
      if (r && r.ok) termPrint("Instant script sent.", 'success');
    } else if (sub === 'abort') {
      await api('/api/cmd', 'POST', {cmd: 'hid_abort_script'});
      termPrint("Script execution aborted.", 'info');
    } else if (sub === 'media') {
      await api('/api/cmd', 'POST', {cmd: 'hid_media', params: {action: subArgs[0]}});
      termPrint(`Sent media key: ${subArgs[0]}`, 'success');
    } else if (sub === 'click') {
      await api('/api/cmd', 'POST', {cmd: 'hid_mouse_click', params: {buttons: parseInt(subArgs[0] || 1)}});
      termPrint(`Mouse clicked: button ${subArgs[0] || 1}`, 'success');
    } else if (sub === 'scroll') {
      await api('/api/cmd', 'POST', {cmd: 'hid_mouse_scroll', params: {wheel: parseInt(subArgs[0] || 0)}});
      termPrint(`Mouse scroll: ${subArgs[0] || 0}`, 'success');
    } else {
      termPrint(`Unknown HID command: ${sub}`, 'error');
    }
  }
  
  else if (menu === 'gps') {
    if (sub === 'on') {
      await api('/api/cmd', 'POST', {cmd: 'gps_on'});
      termPrint("GPS receiver turned ON.", 'success');
    } else if (sub === 'off') {
      await api('/api/cmd', 'POST', {cmd: 'gps_off'});
      termPrint("GPS receiver turned OFF.", 'info');
    } else if (sub === 'status') {
      let r = await api('/api/cmd', 'POST', {cmd: 'gps_status'});
      if (r) {
        let active = r.active ? 'Active' : 'Disabled';
        let fix = r.lat ? `${r.lat}, ${r.lon} (${r.sats} satellites)` : 'No Fix';
        termPrint(`GPS Module: ${active}\nLocation Fix: ${fix}\n`, 'success');
      }
    } else {
      termPrint(`Unknown GPS command: ${sub}`, 'error');
    }
  }
  
  else if (menu === 'wardriving') {
    if (sub === 'start') {
      await api('/api/cmd', 'POST', {cmd: 'gps_wardriving_start'});
      termPrint("Wardriving log active. Writing CSV trip logs to SD card.", 'success');
    } else if (sub === 'stop') {
      await api('/api/cmd', 'POST', {cmd: 'gps_wardriving_stop'});
      termPrint("Wardriving log stopped.", 'info');
    } else {
      termPrint(`Unknown wardriving command: ${sub}`, 'error');
    }
  }
  
  else if (menu === 'pet') {
    if (sub === 'status') {
      let r = await api('/api/cmd', 'POST', {cmd: 'pet_status'});
      if (r) {
        termPrint(`Pet Level: ${r.level} | XP: ${r.xp}\nHP: ${r.health}% | Energy: ${r.energy}%\nCleanliness: ${r.cleanliness}% | Poops: ${r.poops}\n`, 'success');
      }
    } else if (sub === 'feed') {
      await api('/api/cmd', 'POST', {cmd: 'pet_feed'});
      termPrint("Pet fed. Energy increased.", 'success');
    } else if (sub === 'clean') {
      await api('/api/cmd', 'POST', {cmd: 'pet_clean'});
      termPrint("Pet environment cleaned. Poops removed.", 'success');
    } else if (sub === 'heal') {
      await api('/api/cmd', 'POST', {cmd: 'pet_heal'});
      termPrint("Pet treated. HP restored.", 'success');
    } else {
      termPrint(`Unknown pet command: ${sub}`, 'error');
    }
  }
}

async function handleFsCLI(cmd, args) {
  if (cmd === 'ls') {
    let target = resolvePath(args[0] || '');
    let r = await api('/api/cmd', 'POST', {cmd: 'sd_list', params: {path: target}});
    if (r && r.items) {
      let out = `Directory contents of ${target}:\n`;
      r.items.forEach(item => {
        if (item.is_dir) {
          out += `  <span style="color:#00e5ff;font-weight:bold;">[DIR]  ${item.name}</span>\n`;
        } else {
          out += `  [FILE] ${item.name.padEnd(24)} (${formatBytes(item.size)})\n`;
        }
      });
      termPrint(out, 'normal');
    } else {
      termPrint(`ls failed: ${r ? (r.error || 'invalid directory') : 'network error'}`, 'error');
    }
    return;
  }
  
  if (cmd === 'cd') {
    let target = args[0] || '/';
    let resolved = resolvePath(target);
    let r = await api('/api/cmd', 'POST', {cmd: 'sd_list', params: {path: resolved}});
    if (r && r.items) {
      currentDir = resolved;
      updatePrompt();
      termPrint(`Directory changed to: ${currentDir}`, 'success');
    } else {
      termPrint(`cd failed: Directory not found`, 'error');
    }
    return;
  }
  
  if (cmd === 'mkdir') {
    let target = resolvePath(args[0]);
    let r = await api('/api/cmd', 'POST', {cmd: 'sd_mkdir', params: {path: target}});
    if (r && r.ok) {
      termPrint(`Directory created: ${target}`, 'success');
    } else {
      termPrint(`mkdir failed: ${r.error || 'unknown error'}`, 'error');
    }
    return;
  }
  
  if (cmd === 'rm') {
    let target = resolvePath(args[0]);
    let r = await api('/api/cmd', 'POST', {cmd: 'sd_delete', params: {path: target}});
    if (r && r.ok) {
      termPrint(`Deleted: ${target}`, 'success');
    } else {
      termPrint(`rm failed: ${r.error || 'unknown error'}`, 'error');
    }
    return;
  }
  
  if (cmd === 'mv') {
    let src = resolvePath(args[0]);
    let dest = resolvePath(args[1]);
    let r = await api('/api/cmd', 'POST', {cmd: 'sd_rename', params: {old_path: src, new_path: dest}});
    if (r && r.ok) {
      termPrint(`Renamed/Moved ${src} to ${dest}`, 'success');
    } else {
      termPrint(`mv failed: ${r.error || 'unknown error'}`, 'error');
    }
    return;
  }
  
  if (cmd === 'cp') {
    let src = resolvePath(args[0]);
    let dest = resolvePath(args[1]);
    let r = await api('/api/cmd', 'POST', {cmd: 'sd_copy', params: {src: src, dest: dest}});
    if (r && r.ok) {
      termPrint(`Copied ${src} to ${dest}`, 'success');
    } else {
      termPrint(`cp failed: ${r.error || 'unknown error'}`, 'error');
    }
    return;
  }
  
  if (cmd === 'cat') {
    let target = resolvePath(args[0]);
    let r = await api('/api/cmd', 'POST', {cmd: 'sd_read', params: {path: target}});
    if (r && r.content !== undefined) {
      termPrint(escapeHTML(r.content), 'normal');
    } else {
      termPrint(`cat failed: ${r ? (r.error || 'file error') : 'network error'}`, 'error');
    }
    return;
  }
  
  if (cmd === 'write') {
    let target = resolvePath(args[0]);
    let text = args.slice(1).join(' ');
    let r = await api('/api/cmd', 'POST', {cmd: 'sd_write', params: {path: target, content: text}});
    if (r && r.ok) {
      termPrint(`Written file: ${target}`, 'success');
    } else {
      termPrint(`write failed: ${r.error || 'unknown error'}`, 'error');
    }
    return;
  }
  
  if (cmd === 'nano') {
    let target = resolvePath(args[0]);
    if (!args[0]) {
      termPrint("Usage: nano <filename>", 'error');
      return;
    }
    await openNano(target);
    return;
  }
  
  if (cmd === 'upload') {
    termPrint("Upload triggered. Select a file from browser dialog.", 'info');
    document.getElementById('sd-file-input').click();
    return;
  }
  
  if (cmd === 'download') {
    let target = resolvePath(args[0]);
    if (!args[0]) {
      termPrint("Usage: download <filename>", 'error');
      return;
    }
    termPrint(`Downloading: ${target}`, 'success');
    downloadItemPath(target);
    return;
  }
}

function downloadItemPath(fullPath) {
  window.open('/api/sd/download?path=' + encodeURIComponent(fullPath), '_blank');
}

async function handleFallbackCLI(cmd, args) {
  if (cmd === 'deauth') {
    let b = args[0];
    let c = parseInt(args[1]);
    if (!b || isNaN(c)) { termPrint("Usage: deauth <bssid> <ch>", 'error'); return; }
    await api('/api/recon/deauth', 'POST', {bssid: b, ch: c});
    termPrint(`Deauth attack launched on ${b} CH${c}...`, 'danger');
    return;
  }
  if (cmd === 'blackout') {
    await api('/api/cmd', 'POST', {cmd: 'deauth_all'});
    termPrint("Blackout deauth attack launched on all networks...", 'danger');
    return;
  }
  if (cmd === 'sniffer') {
    let c = args[0];
    if (c === 'stop') {
      await api('/api/cmd', 'POST', {cmd: 'sniffer_stop'});
      termPrint("WiFi Promiscuous Sniffer stopped.", 'info');
    } else {
      let ch = parseInt(c);
      if (isNaN(ch)) { termPrint("Usage: sniffer <ch> (or sniffer stop)", 'error'); return; }
      await api('/api/cmd', 'POST', {cmd: 'sniffer_start', params: {ch: ch}});
      termPrint(`WiFi Promiscuous Sniffer active on CH${ch}...`, 'success');
    }
    return;
  }
  if (cmd === 'deauth_detect' || (cmd === 'deauth' && args[0] === 'detect')) {
    await api('/api/cmd', 'POST', {cmd: 'deauth_detect'});
    termPrint("Deauth Detector active. Monitoring mgmt packets...", 'success');
    return;
  }
  if (cmd === 'eviltwin') {
    let ssid = args[0];
    let ch = parseInt(args[1] || 6);
    if (ssid === 'stop') {
      await api('/api/cmd', 'POST', {cmd: 'evil_twin_stop'});
      termPrint("Evil Twin access point stopped.", 'info');
    } else {
      if (!ssid) { termPrint("Usage: eviltwin <ssid> [ch] (or eviltwin stop)", 'error'); return; }
      await api('/api/cmd', 'POST', {cmd: 'evil_twin', params: {ssid: ssid, ch: ch}});
      termPrint(`Evil Twin access point "${ssid}" active on CH${ch}. DNS Redirector active.`, 'success');
    }
    return;
  }
  if (cmd === 'beaconspam') {
    if (args[0] === 'stop') {
      await api('/api/recon/stop', 'POST');
      termPrint("Beacon spam stopped.", 'info');
      return;
    }
    let ssids = args[0];
    if (!ssids) { termPrint("Usage: beaconspam <ssid1,ssid2,...> (or beaconspam stop)", 'error'); return; }
    let list = ssids.split(',');
    await api('/api/cmd', 'POST', {cmd: 'recon_beacon_spam', params: {ssids: list}});
    termPrint(`Beacon spoofer running for: ${list.join(', ')}`, 'success');
    return;
  }
  if (cmd === 'adsb') {
    let act = args[0];
    if (act === 'start') {
      let lat = parseFloat(args[1]);
      let lon = parseFloat(args[2]);
      let name = args[3] || 'Location';
      if (isNaN(lat) || isNaN(lon)) { termPrint("Usage: adsb start <lat> <lon> [name]", 'error'); return; }
      await api('/api/cmd', 'POST', {cmd: 'recon_adsb_start', params: {lat: lat, lon: lon, name: name}});
      termPrint(`ADSB tracking started around ${name} (${lat}, ${lon}).`, 'success');
    } else if (act === 'status') {
      let r = await api('/api/cmd', 'POST', {cmd: 'recon_adsb_status'});
      if (r) {
        termPrint(`ADSB Status: ${r.status || 'idle'}\nActive Aircraft: ${r.aircraft || 'none'}\n`, 'success');
      }
    } else {
      termPrint("Usage: adsb start/status", 'error');
    }
    return;
  }
  if (cmd === 'tesla') {
    await api('/api/cmd', 'POST', {cmd: 'rf_tesla_send'});
    termPrint("Tesla opening code burst sent (433.92 MHz).", 'danger');
    return;
  }
  if (cmd === 'airmouse') {
    let act = args[0];
    if (act === 'start') {
      await api('/api/cmd', 'POST', {cmd: 'hid_airmouse_start'});
      termPrint("Air mouse active.", 'success');
    } else if (act === 'stop') {
      await api('/api/cmd', 'POST', {cmd: 'hid_airmouse_stop'});
      termPrint("Air mouse stopped.", 'info');
    } else if (act === 'cal') {
      await api('/api/cmd', 'POST', {cmd: 'hid_airmouse_calibrate'});
      termPrint("Air mouse calibration triggered.", 'info');
    } else {
      termPrint("Usage: airmouse start/stop/cal", 'error');
    }
    return;
  }
  if (cmd === 'pet') {
    let act = args[0];
    if (act === 'status') {
      let r = await api('/api/cmd', 'POST', {cmd: 'pet_status'});
      if (r) {
        termPrint(`Pet Level: ${r.level} | XP: ${r.xp}\nHP: ${r.health}% | Energy: ${r.energy}%\nCleanliness: ${r.cleanliness}% | Poops: ${r.poops}\n`, 'success');
      }
    } else if (act === 'feed') {
      await api('/api/cmd', 'POST', {cmd: 'pet_feed'});
      termPrint("Pet fed. Energy increased.", 'success');
    } else if (act === 'clean') {
      await api('/api/cmd', 'POST', {cmd: 'pet_clean'});
      termPrint("Pet environment cleaned. Poops removed.", 'success');
    } else if (act === 'heal') {
      await api('/api/cmd', 'POST', {cmd: 'pet_heal'});
      termPrint("Pet treated. HP restored.", 'success');
    } else {
      termPrint("Usage: pet status/feed/clean/heal", 'error');
    }
    return;
  }
  
  let r = await api('/api/cmd', 'POST', {cmd: cmd, params: args});
  if (r && r.ok) {
    if (r.msg) termPrint(r.msg, 'success');
    else termPrint("Command executed successfully.", 'success');
  } else if (r && r.error) {
    termPrint(`Error: ${r.error}`, 'error');
  } else {
    termPrint(`Command not found: ${cmd}. Type 'help' for support.`, 'error');
  }
}

async function openNano(filePath) {
  activeEditorFile = filePath;
  document.getElementById('nano-filename').textContent = filePath;
  document.getElementById('nano-status').textContent = 'Loading...';
  document.getElementById('nano-textarea').value = '';
  
  document.getElementById('terminal-container').style.display = 'none';
  document.getElementById('nano-editor').style.display = 'flex';
  document.getElementById('nano-textarea').focus();
  
  try {
    let r = await api('/api/cmd', 'POST', {cmd: JSON.stringify({cmd:'sd_read',params:{path:filePath}})});
    if (r && r.content !== undefined) {
      document.getElementById('nano-textarea').value = r.content;
      document.getElementById('nano-status').textContent = 'Saved';
    } else {
      document.getElementById('nano-textarea').value = '';
      document.getElementById('nano-status').textContent = 'New File';
    }
  } catch(x) {
    document.getElementById('nano-status').textContent = 'Error Loading';
  }
}

async function saveNanoFile() {
  if (!activeEditorFile) return;
  document.getElementById('nano-status').textContent = 'Saving...';
  let content = document.getElementById('nano-textarea').value;
  try {
    let r = await api('/api/cmd', 'POST', {cmd: JSON.stringify({cmd:'sd_write',params:{path:activeEditorFile,content:content}})});
    if (r && r.ok) {
      document.getElementById('nano-status').textContent = 'Saved';
    } else {
      document.getElementById('nano-status').textContent = 'Save Failed';
    }
  } catch(x) {
    document.getElementById('nano-status').textContent = 'Error Saving';
  }
}

function closeNanoFile() {
  activeEditorFile = '';
  document.getElementById('nano-editor').style.display = 'none';
  document.getElementById('terminal-container').style.display = 'flex';
  termPrint("Exit from nano editor.", 'info');
  document.getElementById('terminal-input').focus();
}

function handleNanoKeys(e) {
  if (e.ctrlKey && e.key.toLowerCase() === 's') {
    e.preventDefault();
    saveNanoFile();
  }
  if (e.ctrlKey && e.key.toLowerCase() === 'x') {
    e.preventDefault();
    closeNanoFile();
  }
}

async function api(url,method,body){
 try{
  let opts={method:method||'GET'};
  if(body){
   if (url === '/api/cmd' && body.cmd) {
     let isJsonString = false;
     if (typeof body.cmd === 'string') {
       let trimmed = body.cmd.trim();
       if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
         isJsonString = true;
       }
     }
     if (!isJsonString) {
       let payload = {cmd: body.cmd};
       if (body.params) {
         payload.params = body.params;
       }
       body = {cmd: JSON.stringify(payload)};
     }
   }
   let fd=new URLSearchParams();
   for(let k in body) fd.append(k,body[k]);
   opts.headers={'Content-Type':'application/x-www-form-urlencoded'};
   opts.body=fd.toString();
  }
  let r=await fetch(url,opts);
  let t=await r.text();
  try{let j=JSON.parse(t);return j}catch(x){return{}}
 }catch(e){return{}}
}

function showTab(t){
 document.querySelectorAll('.tab').forEach(b=>b.classList.remove('active'));
 document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
 document.getElementById(t).classList.add('active');
 document.querySelectorAll('.tab').forEach(b=>{if(b.textContent.toLowerCase().includes(t.substring(0,3)))b.classList.add('active')});
 if(t==='sd') loadSdDir(currentSdPath);
 if(t==='cli') {
   setTimeout(() => {
     document.getElementById('terminal-input').focus();
     updateCursor();
   }, 50);
 }
}

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
  termPrint('Loaded BadUSB script: '+name, 'success');
 }else{
  termPrint('Error loading script: '+(r.error||'unknown'), 'error');
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
  termPrint('Script saved to SD as: '+name, 'success');
 }else{
  termPrint('Error saving script: '+(r.error||'unknown'), 'error');
 }
}
async function runInstantScript(ble){
 let content=document.getElementById('hid-script-box').value;
 let layout=document.getElementById('btn-hid-kb').textContent.replace('⌨️ KB: ','');
 let r=await api('/api/cmd','POST',{cmd:JSON.stringify({cmd:'hid_run_instant',params:{script:content,ble:ble,layout:layout}})});
 if(r&&r.ok){
  termPrint('Instant script sent ('+(ble?'BLE':'USB')+')', 'success');
 }else{
  termPrint('Error running script: '+(r.error||'unknown'), 'error');
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

let currentSdPath = '/';
async function loadSdDir(path) {
  currentSdPath = path;
  el('sd-path', 'PATH: ' + path);
  let list = document.getElementById('sd-file-list');
  list.innerHTML = '<div style="color:#007280;padding:10px;text-align:center;">Loading...</div>';
  try {
    let r = await fetch('/api/sd/list?path=' + encodeURIComponent(path));
    let d = await r.json();
    list.innerHTML = '';
    if (path !== '/') {
      let item = document.createElement('div');
      item.className = 'sd-item';
      item.innerHTML = `
        <span class="name dir" onclick="goUp('${d.parent}')">📁 ... (Go Back)</span>
        <div class="actions"></div>
      `;
      list.appendChild(item);
    }
    if (d.items && d.items.length > 0) {
      d.items.forEach(item => {
        let row = document.createElement('div');
        row.className = 'sd-item';
        let icon = item.isDir ? '📁' : '📄';
        let sizeInfo = item.isDir ? '' : ` (${formatBytes(item.size)})`;
        let displayName = `${icon} ${item.name}${sizeInfo}`;
        
        let nameEl = document.createElement('span');
        nameEl.className = 'name' + (item.isDir ? ' dir' : '');
        nameEl.textContent = displayName;
        if (item.isDir) {
          nameEl.onclick = () => {
            let nextPath = currentSdPath === '/' ? '/' + item.name : currentSdPath + '/' + item.name;
            nextPath = nextPath.replace(/\/+/g, '/');
            loadSdDir(nextPath);
          };
        }
        row.appendChild(nameEl);
        
        let actions = document.createElement('div');
        actions.className = 'actions';
        
        let btnRename = document.createElement('button');
        btnRename.className = 'sd-btn';
        btnRename.textContent = '✏️';
        btnRename.title = 'Rename';
        btnRename.onclick = () => renameItem(item.name);
        actions.appendChild(btnRename);
        
        if (!item.isDir) {
          let btnDl = document.createElement('button');
          btnDl.className = 'sd-btn';
          btnDl.textContent = '📥';
          btnDl.title = 'Download';
          btnDl.onclick = () => downloadItem(item.name);
          actions.appendChild(btnDl);
        }
        
        let btnDel = document.createElement('button');
        btnDel.className = 'sd-btn sd-btn-danger';
        btnDel.textContent = '🗑️';
        btnDel.title = 'Delete';
        btnDel.onclick = () => deleteItem(item.name, item.isDir);
        actions.appendChild(btnDel);
        
        row.appendChild(actions);
        list.appendChild(row);
      });
    } else {
      list.innerHTML += '<div style="color:#007280;padding:10px;text-align:center;">Empty directory</div>';
    }
  } catch(e) {
    list.innerHTML = '<div style="color:#f33;padding:10px;text-align:center;">Error loading directory</div>';
  }
}
function goUp(parentPath) { loadSdDir(parentPath); }
function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  let k = 1024, sizes = ['B', 'KB', 'MB', 'GB'];
  let i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}
function downloadItem(filename) {
  let fullPath = currentSdPath === '/' ? '/' + filename : currentSdPath + '/' + filename;
  window.open('/api/sd/download?path=' + encodeURIComponent(fullPath), '_blank');
}
async function deleteItem(filename, isDir) {
  let fullPath = currentSdPath === '/' ? '/' + filename : currentSdPath + '/' + filename;
  if (confirm(`Are you sure you want to delete ${isDir ? 'folder' : 'file'} "${filename}"?`)) {
    let r = await api('/api/sd/delete', 'POST', {path: fullPath});
    if (r && r.ok) { loadSdDir(currentSdPath); }
  }
}
async function renameItem(oldName) {
  let newName = prompt(`Rename "${oldName}" to:`, oldName);
  if (!newName) return;
  newName = newName.trim();
  if (newName === '' || newName === oldName) return;
  let oldPath = currentSdPath === '/' ? '/' + oldName : currentSdPath + '/' + oldName;
  let newPath = currentSdPath === '/' ? '/' + newName : currentSdPath + '/' + newName;
  let r = await api('/api/sd/rename', 'POST', {path: oldPath, newPath: newPath});
  if (r && r.ok) { loadSdDir(currentSdPath); }
}
async function createFolder() {
  let name = prompt("Enter new folder name:");
  if (!name) return;
  name = name.trim();
  if (name === '') return;
  let r = await api('/api/sd/mkdir', 'POST', {path: currentSdPath, name: name});
  if (r && r.ok) { loadSdDir(currentSdPath); }
}
async function uploadFile() {
  let fileInput = document.getElementById('sd-file-input');
  if (fileInput.files.length === 0) return;
  let file = fileInput.files[0];
  let formData = new FormData();
  formData.append('file', file);
  try {
    let r = await fetch('/api/sd/upload?path=' + encodeURIComponent(currentSdPath), {
      method: 'POST',
      body: formData
    });
    let d = await r.json();
    if (d && d.ok) { loadSdDir(currentSdPath); }
  } catch(e) {}
  fileInput.value = '';
}

document.getElementById('terminal-container').addEventListener('click', () => {
  document.getElementById('terminal-input').focus();
});

window.addEventListener('DOMContentLoaded', async () => {
  termPrint("Initializing SCR Terminal connection...\n", 'info');
  updatePrompt();
  setTimeout(async () => {
    try {
      let r = await api('/api/cmd', 'POST', {cmd: 'status'});
      if (r && r.heap !== undefined) {
        termPrint(getNeofetchHTML(r));
      } else {
        let mockStatus = {
          heap_kb: 320,
          uptime_s: 120,
          bat: 85,
          sd_ok: true,
          sd_size: '32 GB',
          charging: false,
          ip: '192.168.4.1'
        };
        termPrint(getNeofetchHTML(mockStatus));
      }
    } catch(e) {
      termPrint("Welcome to SCR Terminal shell. Type 'help' for command list.\n\n", 'success');
    }
    document.getElementById('terminal-input').focus();
    updateCursor();
  }, 800);
});

wsConnect();
setInterval(()=>{if(ws&&ws.readyState===1)ws.send('ping')},5000);
</script>
</body>
</html>
)rawliteral";
