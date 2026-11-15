// The web page, stored in flash. No external resources: it works on the setup
// network with no internet access.
#pragma once
#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Tower controller</title>
<style>
:root{--bg:#f5f5f7;--card:#fff;--ink:#1d1d1f;--ink2:#6b6b72;--line:#e2e2e7;--accent:#0071e3;--ok:#1f9d55;--warn:#b86e00;--bad:#d43a2f}
@media(prefers-color-scheme:dark){:root{--bg:#000;--card:#1c1c1e;--ink:#f5f5f7;--ink2:#a0a0a8;--line:#333;--accent:#2997ff;--ok:#30d158;--warn:#ffb340;--bad:#ff6961}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:860px;margin:0 auto;padding:20px 14px 60px;display:grid;gap:14px}
h1{font-size:1.6rem;margin:0}h2{font-size:1.05rem;margin:0 0 10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:16px}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:10px}
.stat{background:var(--bg);border-radius:12px;padding:10px 12px}.stat b{display:block;font-size:1.5rem}.stat small{color:var(--ink2)}
.bad b{color:var(--bad)}.muted{color:var(--ink2);font-size:.85rem}
.banner{border-radius:14px;padding:12px 14px;font-weight:600;color:#fff;background:var(--accent)}
.banner.armed{background:var(--ok)}.banner.fault{background:var(--bad)}.banner.paused{background:var(--warn)}
button{font:inherit;border:0;border-radius:999px;padding:8px 14px;background:var(--accent);color:#fff;font-weight:600;cursor:pointer}
button.ghost{background:transparent;color:var(--ink);border:1px solid var(--line)}button.danger{background:var(--bad)}
input,select{font:inherit;padding:8px 10px;border-radius:10px;border:1px solid var(--line);background:var(--bg);color:var(--ink);width:100%}
label{font-size:.75rem;color:var(--ink2);display:block;margin-bottom:3px}
pre{margin:0;max-height:260px;overflow:auto;font-size:.78rem;white-space:pre-wrap}
#msg{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--ink);color:var(--bg);padding:9px 16px;border-radius:999px;font-weight:600;display:none}
</style></head><body><main>
<div class="row" style="justify-content:space-between"><h1>Tower controller</h1><span class="muted" id="meta"></span></div>
<div class="banner" id="banner">…</div>

<div class="card"><h2>Readings</h2><div class="grid" id="readings"></div><p class="muted" id="age"></p></div>

<div class="card"><h2>Dosing</h2>
<p class="muted" id="hist"></p>
<div class="row"><button id="arm">Arm real dosing</button><button class="ghost" id="pause">Pause</button><button class="danger" id="clear" hidden>Clear fault</button><button class="danger" id="stop">Stop pumps</button></div>
<p class="muted">Shadow mode logs what would be dosed and runs no pump. Keep it for two weeks and compare with what you do by hand before arming.</p></div>

<div class="card"><h2>Calibration</h2>
<p class="muted">pH: probe in the buffer for one minute, then save the point. Two buffers are needed (for example 6.86 and 4.00).</p>
<div class="grid"><div><label>Buffer pH, point 1</label><input id="ph1" type="number" step="any" value="6.86"></div><div><label>&nbsp;</label><button data-cal="1">Save point 1</button></div>
<div><label>Buffer pH, point 2</label><input id="ph2" type="number" step="any" value="4.00"></div><div><label>&nbsp;</label><button data-cal="2">Save point 2</button></div></div>
<p class="muted" id="phcal"></p>
<div id="ecbox"><p class="muted">EC: probe in a solution of known conductivity (1.413 mS/cm calibration solution), wait for a fresh reading, then save.</p>
<div class="grid"><div><label>Known EC, mS/cm</label><input id="eck" type="number" step="any" value="1.413"></div><div><label>&nbsp;</label><button id="eccal">Calibrate EC</button></div></div>
<p class="muted" id="eccalinfo"></p></div>
<p class="muted">Pumps: run each one for 60 s into a measuring cup of plain water, then enter the millilitres collected.</p>
<div id="pumps"></div></div>

<div class="card"><h2>Settings</h2>
<div class="grid">
<div><label>Tank volume, L</label><input id="tankL" type="number" step="any"></div>
<div><label>Dose pH Down above pH</label><input id="phHigh" type="number" step="any"></div>
<div><label>pH Down per dose, mL</label><input id="phStepMl" type="number" step="any"></div>
<div><label>Top up nutrients below EC</label><input id="ecLow" type="number" step="any"></div>
<div><label>Top-up Micro, mL/L</label><input id="top0" type="number" step="any"></div>
<div><label>Top-up Grow, mL/L</label><input id="top1" type="number" step="any"></div>
<div><label>Top-up Bloom, mL/L</label><input id="top2" type="number" step="any"></div>
<div><label>pH dosing</label><select id="phDosing"><option value="1">on</option><option value="0">off</option></select></div>
<div><label>Nutrient dosing</label><select id="nutrientDosing"><option value="1">on</option><option value="0">off</option></select></div>
<div><label>12 V circulation pump on output 5</label><select id="circulation"><option value="0">not used</option><option value="1">on</option></select></div>
</div><div class="row" style="margin-top:10px"><button id="save">Save settings</button></div>
<p class="muted">Default top-up doses are for Terra Aquatica TriPart and raise EC by about 0.2. The daily ceilings are fixed in the firmware and cannot be changed here.</p></div>

<div class="card"><h2>Wi-Fi</h2><div class="grid"><div><label>Network name</label><input id="ssid"></div><div><label>Password</label><input id="pass" type="password"></div><div><label>&nbsp;</label><button id="wifi">Save and restart</button></div></div><p class="muted" id="wifiinfo"></p></div>

<div class="card"><h2>Log</h2><pre id="log"></pre></div>
</main><div id="msg"></div>
<script>
var S=null,filled=false,PN=['Micro','Grow','Bloom','pH Down'];
function $(i){return document.getElementById(i)}
function say(t){var m=$('msg');m.textContent=t;m.style.display='block';clearTimeout(say.t);say.t=setTimeout(function(){m.style.display='none'},3000)}
function post(path,data){var b=new URLSearchParams(data||{});return fetch(path,{method:'POST',headers:{'X-Tower':'1','Content-Type':'application/x-www-form-urlencoded'},body:b}).then(function(r){return r.json()}).then(function(j){say(j.msg);refresh();return j}).catch(function(){say('no answer from the controller')})}
function dur(s){return s>=3600?Math.floor(s/3600)+' h '+Math.floor(s%3600/60)+' min':(s>=60?Math.floor(s/60)+' min':s+' s')}
function stat(k,v,u,bad){return '<div class="stat'+(bad?' bad':'')+'"><small>'+k+'</small><b>'+v+'</b><small>'+u+'</small></div>'}
function render(){var m=S.meas,c=S.cal,b=$('banner');
 $('meta').textContent=S.board+' · up '+dur(S.uptime)+' · '+S.wifi.ip;
 b.className='banner'+(S.locked?' fault':(S.paused?' paused':(S.mode=='armed'?' armed':'')));
 b.textContent=S.locked?'FAULT: '+S.fault+'. Dosing is disabled until you clear it.':(m.tankEmpty?'Tank empty: pumps stopped.':(S.paused?'Paused.':(S.mode=='armed'?'Armed: automatic dosing is real.':'Shadow mode: decisions are logged, no pump runs.')));
 $('readings').innerHTML=stat('pH',m.phValid?m.ph.toFixed(2):'–',c.phDone?m.phV.toFixed(3)+' V':'not calibrated',!m.phValid)+
  (S.hasEc?stat('EC',m.ecValid?m.ec.toFixed(2):'–',c.ecDone?'mS/cm':'not calibrated',!m.ecValid):'')+
  stat('Water',m.tempValid?m.temp.toFixed(1):'–','°C',!m.tempValid)+
  stat('Level',m.tankEmpty?'EMPTY':(m.levelLow?'LOW':'ok'),'float switches',m.tankEmpty||m.levelLow)+
  (m.v12!=null?stat('Supply',m.v12.toFixed(1),'V',m.v12<11):'');
 $('age').textContent=m.any?'Measured '+dur(m.age)+' ago. A new reading every 30 s.':'Waiting for the first measurement.';
 $('hist').textContent='Last 24 h: '+S.hist.phDoses+' of '+S.limits.phDoses+' pH doses ('+S.hist.phMl+' of '+S.limits.phMl+' mL), '+S.hist.topups+' of '+S.limits.topups+' nutrient top-ups. Next pH dose possible in '+dur(S.wait.ph)+', next top-up in '+dur(S.wait.topup)+'.';
 $('arm').textContent=S.mode=='armed'?'Back to shadow mode':'Arm real dosing';$('pause').textContent=S.paused?'Resume':'Pause';$('clear').hidden=!S.locked;
 $('phcal').textContent=c.phDone?'Calibrated: pH '+c.ph1+' at '+c.v1+' V, pH '+c.ph2+' at '+c.v2+' V.':'Not calibrated yet.';
 $('ecbox').hidden=!S.hasEc;$('eccalinfo').textContent=c.ecDone?'Calibrated, factor '+c.ecK+'.':'Not calibrated yet.';
 if(!filled){filled=true;var s=S.set;['tankL','phHigh','phStepMl','ecLow'].forEach(function(k){$(k).value=s[k]});[0,1,2].forEach(function(i){$('top'+i).value=s.topup[i]});
  $('phDosing').value=s.phDosing?1:0;$('nutrientDosing').value=s.nutrientDosing?1:0;$('circulation').value=s.circulation?1:0;
  $('pumps').innerHTML=PN.map(function(n,i){return '<div class="grid" style="margin-bottom:8px"><div><label>'+n+'</label><button class="ghost" data-run="'+i+'">Run 60 s</button></div><div><label>mL collected in 60 s</label><input id="ml'+i+'" type="number" step="any"></div><div><label>&nbsp;</label><button data-pcal="'+i+'">Save</button></div><div><label>Flow</label><span id="fl'+i+'" class="muted"></span></div></div>'}).join('')}
 PN.forEach(function(n,i){var e=$('fl'+i);if(e)e.textContent=c.flow[i]>=1?c.flow[i]+' mL/min':'not calibrated'});
 $('wifiinfo').textContent=S.wifi.ap?'Setup network active. Enter your home Wi-Fi to connect the controller.':'Connected. Address: '+S.wifi.ip;
 $('log').textContent=S.log.map(function(l){return '['+dur(S.uptime-l.t)+' ago] '+l.m}).join('\n')}
function refresh(){return fetch('/api/status').then(function(r){return r.json()}).then(function(j){S=j;render()}).catch(function(){})}
document.addEventListener('click',function(e){var t=e.target;if(!S||t.tagName!='BUTTON')return;
 if(t.id=='arm'){if(S.mode=='armed')post('/api/mode',{armed:0});else if(window.confirm('Arm real dosing? Pumps will add pH Down and nutrients on their own.'))post('/api/mode',{armed:1})}
 else if(t.id=='pause')post('/api/pause',{on:S.paused?0:1});
 else if(t.id=='clear')post('/api/fault/clear');
 else if(t.id=='stop')post('/api/pump/stop');
 else if(t.dataset.cal)post('/api/cal/ph',{point:t.dataset.cal,ph:$('ph'+t.dataset.cal).value});
 else if(t.id=='eccal')post('/api/cal/ec',{ec:$('eck').value});
 else if(t.dataset.run)post('/api/pump/run',{i:t.dataset.run,s:60});
 else if(t.dataset.pcal)post('/api/pump/cal',{i:t.dataset.pcal,ml:$('ml'+t.dataset.pcal).value,s:60});
 else if(t.id=='save'){var d={};['tankL','phHigh','phStepMl','ecLow','top0','top1','top2','phDosing','nutrientDosing','circulation'].forEach(function(k){d[k]=$(k).value});post('/api/settings',d)}
 else if(t.id=='wifi')post('/api/wifi',{ssid:$('ssid').value,pass:$('pass').value})});
refresh();setInterval(refresh,5000);
</script></body></html>)HTML";
