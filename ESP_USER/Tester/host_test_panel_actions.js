'use strict';
// Exercise the firmware's real script and inline controls, without preview shims.
const fs = require('fs');
const path = require('path');
const assert = require('assert/strict');
const {JSDOM} = require('jsdom');
const source = fs.readFileSync(path.join(__dirname, '..', 'simple_panel.h'), 'utf8');
const html = source.split('R"rawliteral(')[1].split(')rawliteral"')[0];
const dom = new JSDOM(html, {runScripts:'outside-only', url:'http://panel.test/'});
const w = dom.window, d = w.document;
w.setInterval = () => 0;
w.addEventListener = () => {};
w.fetch = async () => {throw Error('offline');};
w.eval(html.match(/<script>([\s\S]*?)<\/script>/)[1]);
const tick = () => new Promise(resolve => setImmediate(resolve));
const response = (status, body) => ({status, ok:status === 200, headers:{get:()=>null},
    json:async()=>body, text:async()=>typeof body === 'string' ? body : JSON.stringify(body)});
let checks = 0;
function check(value, message) {assert.ok(value, message); checks++; console.log('PASS: '+message);}
function click(button) {w.eval('(function(){'+button.getAttribute('onclick')+'})').call(button);}
(async()=>{
    await tick();
    check(Buffer.byteLength(html) <= 175000, 'panel stays within literal budget');
    check(d.styleSheets.length === 1 && d.styleSheets[0].cssRules.length > 100, 'complete stylesheet parses, including mobile media queries');
    check([...d.styleSheets[0].cssRules].some(r=>r.selectorText === '.glyph'), 'line icon styles survive stylesheet parsing');
    check(d.getElementById('adminCard').hidden && d.getElementById('admGate').hidden, 'admin remains hidden');
    const icons = [...d.querySelectorAll('.icog button')];
    check(icons.length === 3 && icons.every(b=>b.querySelector('svg use') && b.getAttribute('aria-label')), 'three named local line icons');
    check([...d.querySelectorAll('svg use')].every(u=>d.querySelector(u.getAttribute('href'))), 'every SVG reference resolves locally');
    check(d.getElementById('toastMsg').getAttribute('role') === 'status', 'feedback announced accessibly');
    for(const el of d.querySelectorAll('[onclick]')) w.Function(el.getAttribute('onclick'));
    check(true, 'all static inline click handlers compile');
    let finish, calls = [];
    w.fetch = (url, options) => {calls.push([url, options]); return new Promise(r=>{finish=r;});};
    click(icons[1]);click(icons[1]);click(icons[0]);
    check(calls.length === 1 && icons.every(b=>b.disabled), 'toolbar blocks repeat clicks and conflicting refresh');
    finish(response(200,{fs:1,total:64000,used:2000,files:[{name:'/evt-20261104.csv',size:2000}]}));
    await tick();
    check(icons.every(b=>!b.disabled && !b.hasAttribute('aria-busy')), 'toolbar unlocks after response');
    check(d.getElementById('toastMsg').textContent.includes('به‌روز'), 'refresh success feedback');
    const dayButton = d.querySelector('#evtFs button');
    w.Function(dayButton.getAttribute('onclick'));
    let file;
    w.downloadCsvText = (name,text) => {file={name,text};};
    calls=[];click(dayButton);click(dayButton);
    check(calls.length === 1 && calls[0][0] === '/evt/dl?day=20261104', 'per-item download fetches only selected day once');
    check(dayButton.disabled && !file, 'download waits for file before success');
    finish(response(200,'# header\n1700000000,1\n'));await tick();
    check(file && file.text.includes('1700000000,1') && !dayButton.disabled, 'complete CSV delivered and button restored');
    file=null;click(icons[2]);finish(response(500,'failed'));await tick();
    check(!file && !icons[2].disabled && d.getElementById('toastMsg').textContent.includes('انجام نشد'), 'HTTP failure never downloads an error page');
    calls=[];click(icons[2]);finish(response(200,'# all days\nrow1\nrow2'));await tick();
    check(calls[0][0] === '/evt/dl?all=1' && file.text.includes('row2'), 'all-days download is one complete request');
    w.admUnlock('admin');
    d.getElementById('admSsid').value='test-network';d.getElementById('admWpass').value='secret123';
    const save = [...d.querySelectorAll('#adminCard button')].find(b=>b.textContent==='ذخیره');
    calls=[];click(save);click(save);
    check(calls.length === 1 && d.querySelectorAll('#adminCard button:disabled').length > 5, 'admin mutations blocked while save pending');
    check(calls[0][0] === '/wifi' && calls[0][1].method === 'POST', 'save retains authenticated POST route');
    // Avoid the post-save read of /wifi interfering with deferred requests.
    w.loadWifi=()=>{};
    finish(response(200,{ok:1}));await tick();
    check(!save.disabled && !d.getElementById('admWpass').value && d.getElementById('toastMsg').textContent.includes('ذخیره شد'), 'save confirms success, clears password and unlocks');
    d.getElementById('admWpass').value='short';calls=[];click(save);await tick();
    check(!calls.length && !save.disabled, 'invalid input unlocks without sending');
    d.getElementById('admWpass').value='secret123';click(save);finish(response(403,{ok:0,e:'admin'}));await tick();
    check(d.getElementById('adminCard').hidden && !save.disabled, 'expired session hides admin and releases busy state');
    w.admUnlock('admin');
    w.fetch=async()=>{throw Error('offline');};click(save);await tick();
    check(!save.disabled && d.getElementById('toastMsg').textContent.includes('ارتباط'), 'network failure has feedback and unlocks');
    // Accelerate only I/O deadlines, not toast timers.
    const realTimeout=w.setTimeout.bind(w);
    w.setTimeout=(fn,ms)=>realTimeout(fn,ms===10000||ms===15000?1:ms);
    w.fetch=(url,options)=>Promise.resolve({status:200,ok:true,headers:{get:()=>null},
        text:()=>new Promise((resolve,reject)=>options.signal.addEventListener('abort',()=>reject(Error('timeout')))),
        json:()=>new Promise((resolve,reject)=>options.signal.addEventListener('abort',()=>reject(Error('timeout'))))});
    await w.uiAction(save,()=>w.admWifi(),true);
    check(!save.disabled && d.getElementById('toastMsg').textContent.includes('ارتباط'), 'hung admin response body times out and unlocks');
    file=null;await w.uiAction(icons[2],()=>w.evtDlAll());
    check(!file && !icons[2].disabled, 'hung download body times out and unlocks');
    w.admOpen();d.getElementById('admPass').value='wrong';calls=[];
    w.fetch=(url,options)=>{calls.push([url,options]);return new Promise(r=>{finish=r;});};
    const login=d.getElementById('admLoginBtn');click(login);click(login);
    check(calls.length===1 && login.disabled, 'repeat login blocked');
    finish(response(429,{ok:0}));await tick();
    check(!login.disabled && d.getElementById('admMsg').textContent.includes('صبر'), 'login lockout remains clear and usable');
    console.log(`ALL ${checks} PANEL ACTION CHECKS PASSED`);
})().then(()=>{dom.window.close();}).catch(error=>{console.error(error);dom.window.close();process.exitCode=1;});
