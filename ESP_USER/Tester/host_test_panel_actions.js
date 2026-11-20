'use strict';
// Exercise the firmware's real script and inline controls, without preview shims.
const fs = require('fs');
const path = require('path');
const assert = require('assert/strict');
const {JSDOM} = require('jsdom');
const source = fs.readFileSync(path.join(__dirname, '..', 'simple_panel.h'), 'utf8');
const html = source.split('R"rawliteral(')[1].split(')rawliteral"')[0];
const dom = new JSDOM(html, {runScripts:'outside-only', pretendToBeVisual:true, url:'http://panel.test/'});
const w = dom.window, d = w.document;
w.setInterval = () => 0;
w.addEventListener = () => {};
w.fetch = async () => {throw Error('offline');};
w.eval(html.match(/<script>([\s\S]*?)<\/script>/)[1]+'\nwindow.resetStatusState=()=>{g_inputState=null;g_inputOV=false;g_fullActive=false;[runPackState,runLowState,runHighState,chgPackState,chgLowState,chgHighState].forEach(s=>s.init=false);};');
const tick = () => new Promise(resolve => setImmediate(resolve));
const response = (status, body) => ({status, ok:status === 200, headers:{get:()=>null},
    json:async()=>body, text:async()=>typeof body === 'string' ? body : JSON.stringify(body)});
let checks = 0;
function check(value, message) {assert.ok(value, message); checks++; console.log('PASS: '+message);}
function click(button) {w.eval('(function(){'+button.getAttribute('onclick')+'})').call(button);}
(async()=>{
    await tick();
    const stepLabel=w.evtTextFa({chg:1024,note:0});
    check(stepLabel.txt==='ولتاژ پک یک پلهٔ ۵۰۰ میلی‌ولتی جابه‌جا شد','voltage-step label explicitly names 500 mV');
    check(w.evtTextFa({chg:0,note:7}).txt==='شروع روز','day-start marker has its own Persian label');
    const parsed=w.evtParse('1793836800,60,512,0,1,4,1,2,256,18,12000,12500,2,28000,100,200,300,400');
    check(parsed.vlow===12000 && parsed.vhigh===12500 && parsed.v24===24500 && parsed.dm===2 && parsed.vin===28000 && parsed.i1===100 && parsed.i2===200 && parsed.d1===300 && parsed.d2===400 && !('pct' in parsed),'18-column row is decoded in calibration order without battery percent');
    const config=fs.readFileSync(path.join(__dirname,'..','simple_config.h'),'utf8');
    const step=Number(config.match(/#define\s+SIMPLE_LOG_VSTEP_MV\s+(\d+)u/)[1]);
    const preview=fs.readFileSync(path.join(__dirname,'..','simple_preview.html'),'utf8');
    check(step===500 && preview.includes('EVT_VSTEP_MV='+step+','),'generated preview log step matches the 500 mV firmware setting');
    check(Buffer.byteLength(html) <= 175000, 'panel stays within literal budget');
    check(!d.getElementById('labBar') && !html.includes('نوار شیمیولیتور'), 'user panel never includes the simulator bottom strip');
    check(d.styleSheets.length === 1 && d.styleSheets[0].cssRules.length > 100, 'complete stylesheet parses, including mobile media queries');
    check([...d.styleSheets[0].cssRules].some(r=>r.selectorText === '.glyph'), 'line icon styles survive stylesheet parsing');
    check(d.querySelectorAll('#evtCard').length===1 && d.getElementById('evtCard').closest('#settingsContent') && !d.querySelector('#pg0 #evtCard,#pg1 #evtCard'),'one log card lives in settings, not status or charts');
    for(const n of [0,1,2,0]){
        w.showTab(n);
        check(d.getElementById('evtCard').closest('.pgx').classList.contains('a')===(n===2),'log follows only the settings tab: '+n);
    }
    function statusFrame({input=true,pack=25000,state=0,fault=0,flags=0,dead=0,imb=0,on=true,vin,age=100,n=12,ce=2}={}){
        w.resetStatusState();
        const t=Array(31).fill(0);t[14]=vin===undefined?(input?24000:0):vin;t[15]=pack;t[17]=pack/2;t[18]=pack/2;t[6]=state;t[13]=state;t[19]=fault;t[25]=imb;t[28]=dead;
        w.render({t,on,n,age,ce,fl:input?2:0,fl2:flags,inputPresent:input,lko:1,bpv:2,brx:10,boardTime:1793793600});
    }
    check(!d.querySelector('#pg1 [onclick*="exportCsv"]') && !d.getElementById('pg1').textContent.includes('CSV'),'charts have no CSV export controls');
    check(['exportCsv','chargeCsvData','voltCsvData','csvRowsFromColumns','csvText','csvCell','csvFileStamp','jLatinDate'].every(name=>!html.includes('function '+name+'(')),'chart-only CSV builders and unused date helper are removed');
    check(typeof w.downloadCsvText==='function' && d.querySelector('#pg2 #evtCard [onclick*="evtDlAll"]'),'stored history downloads remain available in settings');
    const tech=d.getElementById('technicalInfo');
    check(tech.closest('#settingsContent') && tech.closest('#adminCard') && d.querySelectorAll('#linkState,#seqInfo').length===2,'technical status exists once, inside the admin panel only');
    check(d.getElementById('linkState').closest('[lang="en"][dir="ltr"]')===d.getElementById('seqInfo').closest('[lang="en"][dir="ltr"]'),'English technical values have an explicit left-to-right layout');
    check(!d.querySelector('.footer #linkState,.footer #seqInfo') && d.querySelector('.footer').children.length===1 && d.querySelector('.footer #fwTap[onclick="admTap()"]'),'footer retains only version and its hidden admin entry');
    for(const tab of [0,1,2,0]){
        w.showTab(tab);
        check(tech.closest('.pgx').classList.contains('a')===(tab===2),'technical information follows settings visibility: '+tab);
    }
    statusFrame();
    check(d.getElementById('linkState').textContent==='Link: Connected | CRC errors: 2 | Vin: 24.00 V','technical link, CRC errors and input voltage use live English labels');
    check(d.getElementById('seqInfo').textContent.includes('Frames: 12 | Last update: 100 ms ago') && !/[\u0600-\u06ff]/.test(d.getElementById('seqInfo').textContent),'last update means time since the latest frame, with clear English units');
    statusFrame({on:false,age:700,n:13,ce:3});
    check(d.getElementById('linkState').textContent.startsWith('Link: Disconnected | CRC errors: 3') && d.getElementById('seqInfo').textContent.includes('Frames: 13 | Last update: 700 ms ago'),'technical values continue updating after link loss');
    statusFrame({state:1});
    check(d.getElementById('seqInfo').textContent.includes('Charging range:') && !/[\u0600-\u06ff]/.test(tech.querySelector('[lang="en"]').textContent),'charging-stage diagnostics are also English');
    const mainTexts=()=>['statusTitle','statusSub','bdgInput','heroFault'].map(id=>{const e=d.getElementById(id);return e.style.display==='none'?'':e.textContent;}).join(' | ');
    for(const pack of [21000,21300,22000,25000,29000]){
        statusFrame({input:false,pack});
        check((mainTexts().match(/برق ورودی: قطع/g)||[]).length===1 && !/برق.*قطع/.test(d.getElementById('statusTitle').textContent+d.getElementById('heroFault').textContent),'power outage appears once at pack voltage '+pack);
        check((d.getElementById('heroFault').style.display!=='none')===(pack<=22600),'only real low-charge warnings remain at pack voltage '+pack);
    }
    statusFrame({input:false,pack:21000});statusFrame({input:false,pack:25000});
    check(d.getElementById('heroFault').style.display==='none' && !d.getElementById('heroFault').textContent && d.getElementById('statusSub').style.display==='none','recovery clears and hides the previous warning and empty subtitle');
    statusFrame({state:3});
    check(d.getElementById('statusTitle').textContent==='شارژ کامل' && (mainTexts().match(/شارژ کامل/g)||[]).length===1 && d.getElementById('heroFault').style.display==='none','charge complete is shown once without a second success banner');
    statusFrame();
    check(d.getElementById('statusTitle').textContent==='آماده به کار' && d.getElementById('heroFault').style.display==='none' && !d.getElementById('statusSub').textContent,'normal idle mode adds no redundant power, clock or percentage message');
    statusFrame({state:1});
    check(d.getElementById('statusTitle').textContent==='در حال شارژ' && d.getElementById('statusSub').textContent.includes('قسمت') && d.getElementById('heroFault').style.display==='none','charging keeps one mode and the distinct charge-stage detail');
    statusFrame({state:10});
    check(d.getElementById('statusTitle').textContent.includes('استراحت حرارتی') && d.getElementById('statusSub').textContent.includes('۱۰ دقیقه'),'thermal rest still shows its duration');
    statusFrame({vin:30000});
    check(d.getElementById('bdgInput').textContent.includes('اضافه ولتاژ') && d.getElementById('statusSub').textContent.includes('حد 28.0 ولت') && d.getElementById('heroFault').style.display==='none','overvoltage keeps actual input and live threshold without a duplicate alert');
    statusFrame({on:false});
    check(d.getElementById('statusTitle').textContent.includes('ارتباط با برد قطع') && d.getElementById('statusSub').textContent.includes('UART') && d.getElementById('heroFault').style.display==='none','link-loss warning and action survive without a repeated banner');
    for(const scenario of [{dead:1},{flags:2},{flags:16},{flags:1,imb:450}]){
        statusFrame(scenario);
        check(!!d.getElementById('statusSub').textContent && !d.getElementById('imbBox').classList.contains('show') && d.getElementById('heroFault').style.display==='none','primary battery lock or imbalance action is not duplicated in another box');
    }
    statusFrame({input:false,imb:450});
    check(d.getElementById('imbBox').classList.contains('show') && d.getElementById('imbBox').textContent.includes('450') && d.getElementById('heroFault').style.display==='none','separate imbalance warning remains visible during power loss');
    for(const fault of [1,64,128]){
        statusFrame({fault});
        check(!!d.getElementById('statusSub').textContent && d.getElementById('heroFault').style.display==='none','a single primary fault has one action without a second banner: '+fault);
    }
    statusFrame({fault:64|2});
    check(d.getElementById('statusTitle').textContent==='باتری قطع شده' && !d.getElementById('heroFault').textContent.includes('باتری قطع شده') && d.getElementById('heroFault').textContent.includes('جریان بیش از حد'),'simultaneous distinct faults are preserved, excluding the repeated primary fault');
    check(['cCharge','cVolt'].every(id=>d.getElementById(id).closest('.chart-card')),'both remaining charts use dedicated flat-color cards');
    const canvasRule=[...d.styleSheets[0].cssRules].find(r=>r.selectorText==='canvas');
    const cardRule=[...d.styleSheets[0].cssRules].find(r=>r.selectorText==='.cd.chart-card');
    check(canvasRule.style.getPropertyValue('background-image')==='none' && cardRule.style.getPropertyValue('background')==='rgb(18, 24, 38)' && cardRule.style.getPropertyValue('backdrop-filter')==='none','chart surfaces use solid CSS colors without an image or glass blur');
    let strokes=0,fills=0;
    const ctx=new Proxy({}, {get:(target,key)=>key in target?target[key]:key==='createLinearGradient'||key==='drawImage'?()=>{throw Error('chart attempted gradient/image');}:key==='stroke'?()=>strokes++:key==='fill'?()=>fills++:()=>{},set:(target,key,value)=>{target[key]=value;return true;}});
    for(const id of ['cCharge','cVolt']){const c=d.getElementById(id);c.getContext=()=>ctx;Object.defineProperty(c,'clientWidth',{get:()=>700});Object.defineProperty(c,'clientHeight',{get:()=>180});}
    w.showTab(1);await tick();
    const times=[1700000000,1700003600];
    w.drawVoltChart('cVolt',[24000,25000],[23000,24000],times,'1d');
    w.drawChargeHistory([{ts:Math.floor(Date.now()/1000),ch:1}],'1d');
    check(strokes>3 && fills>0,'live chart axes, lines and events render without gradients or image drawing');
    check(!d.getElementById('cPct') && !d.getElementById('powerLog') && !html.includes('۲۰ قطع و وصل آخر') && !html.includes('درصد شارژ پک - با زمان واقعی'),'percentage chart and recent power card are removed, not hidden');
    check(!html.includes('chartDataPct') && !html.includes('pollChartsPct') && !html.includes('pctCsvData') && !html.includes('segPct'),'percentage chart data, polling, export and range controls are removed');
    let chartCalls=[];
    w.fetch=async url=>{chartCalls.push(url);return response(200,{range:new URL(url,'http://panel.test').searchParams.get('range'),vin:[24000,25000],vpack:[23000,24000],time:times});};
    await w.pollCharts();
    check(chartCalls.length===1 && chartCalls[0]==='/charts?range=1d','chart polling sends just one request for voltage');
    for(const range of ['1d','7d','1m','3m']){
        click(d.getElementById('volt'+range));
        await tick();
        check(d.getElementById('volt'+range).classList.contains('a') && chartCalls.at(-1)==='/charts?range='+range,'voltage ranges work beside the fixed thirty-day calendar: '+range);
    }
    w.renderDiag({valid:true,totalChargeSec:12,uptimeSec:34,chargeCount:8,chargeCountCh1:3,chargeCountCh2:5,resetCount:9,power:[{off:1700000000,on:1700000100,dur:100}],chargeHistory:[{ts:Math.floor(Date.now()/1000),ch:2}]});
    check(d.getElementById('batHighChargeCount').textContent==='3' && d.getElementById('batLowChargeCount').textContent==='5' && d.querySelector('#chargeHistoryList .row') && !d.getElementById('powerLog'),'per-battery completed-charge counts and charge history still render independently');
    check(!d.querySelector('#stChargeCount,#stResets') && !/ریست|تعداد شارژ/.test(d.getElementById('boardInfo').textContent),'reset count and combined charge count are removed from statistics and board details');
    check(['batHighChargeCount','batLowChargeCount'].every(id=>d.getElementById(id).closest('#pg0') && d.getElementById(id).nextElementSibling.textContent==='تعداد شارژ کامل'),'completed-charge counters exist only on the two main battery cards with explicit labels');
    w.renderDiag({valid:true,totalChargeSec:12,uptimeSec:34,chargeCount:0,chargeCountCh1:0,chargeCountCh2:0,resetCount:0});
    check(d.getElementById('batHighChargeCount').textContent==='0' && d.getElementById('batLowChargeCount').textContent==='0','zero completed charges remain a visible number rather than a dash');
    const clockDom=new JSDOM(html,{runScripts:'outside-only',url:'http://clock.test/'});
    const cw=clockDom.window, hooks={},docHooks={},intervals=[],timeouts=new Map();let timerId=0,clockRequests=[],clockFailure=false,clockHold=null;
    cw.addEventListener=(name,fn)=>{hooks[name]=fn;};
    cw.document.addEventListener=(name,fn)=>{docHooks[name]=fn;};
    Object.defineProperty(cw.document,'hidden',{get:()=>false});
    cw.setInterval=(fn,ms)=>{intervals.push([fn,ms]);return 0;};
    cw.setTimeout=(fn,ms)=>{const id=++timerId;timeouts.set(id,[fn,ms]);return id;};
    cw.clearTimeout=id=>timeouts.delete(id);
    cw.fetch=async(url,options)=>{
        if(url.startsWith('/clock?')){
            clockRequests.push([url,options]);
            if(clockHold) await clockHold;
            if(clockFailure)return response(503,{ok:0});
            return response(200,{ok:1});
        }
        return response(200,'');
    };
    cw.eval(html.match(/<script>([\s\S]*?)<\/script>/)[1]);await tick();
    check(clockRequests.length===1 && clockRequests[0][1].method==='POST' && !clockRequests[0][1].headers['X-Admin-Token'],'opening the page automatically syncs without login or a click');
    const clockUrl=new URL(clockRequests[0][0],'http://clock.test');
    check(Math.abs(+clockUrl.searchParams.get('sec')-Date.now()/1000)<3 && +clockUrl.searchParams.get('tz')===-new Date().getTimezoneOffset()*60,'automatic sync reads actual system seconds and timezone');
    check(!cw.document.querySelector('[onclick*="admClock"]') && !html.includes('function admClock'),'manual synchronization button and handler are removed');
    for(const callback of [hooks.focus,hooks.online,docHooks.visibilitychange,intervals.find(x=>x[1]===60000)[0]]){const before=clockRequests.length;callback();await tick();check(clockRequests.length===before+1,'clock resyncs automatically on resume, reconnect or periodic timer');}
    cw.admTokenSet('a'.repeat(64));await cw.evtClock();
    check(clockRequests.at(-1)[1].headers['X-Admin-Token']==='a'.repeat(64),'the same automatic sync works while admin is logged in');
    clockFailure=true;await cw.evtClock();
    let retry=[...timeouts.values()].find(x=>x[1]===5000);
    check(!!retry,'failed automatic clock sync schedules a five-second retry');
    clockFailure=false;const beforeRetry=clockRequests.length;await retry[0]();
    check(clockRequests.length===beforeRetry+1 && ![...timeouts.values()].some(x=>x[1]===5000),'retry succeeds automatically and clears the retry timer');
    let releaseClock;clockHold=new Promise(r=>releaseClock=r);
    const heldClock=cw.evtClock(),beforeBusy=clockRequests.length;await cw.evtClock();
    check(clockRequests.length===beforeBusy,'simultaneous reconnect and focus do not duplicate an in-flight sync');
    releaseClock();await heldClock;clockDom.window.close();
    check(d.getElementById('adminCard').hidden && d.getElementById('admGate').hidden, 'admin remains hidden');
    const icons = [...d.querySelectorAll('.icog button')];
    check(icons.length === 4 && icons[3].hidden && icons.every(b=>b.querySelector('svg use') && b.getAttribute('aria-label')), 'three public icons and a hidden named format icon');
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
    d.getElementById('admWpass').value='secret123';
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
    // Browser refuses every cookie: protected actions must use the login token.
    const token='a'.repeat(64);let active=false,formatted=false;
    const server=async(url,options={})=>{
        const owns=active && options.headers && options.headers['X-Admin-Token']===token;
        if(url==='/login'){
            if(active&&!owns)return response(409,{ok:0});
            active=true;return response(200,{ok:1,user:'admin',token});
        }
        if(url==='/logout'){if(owns)active=false;return response(200,{ok:1});}
        if(url==='/evt/fs')return response(200,{fs:1,total:65536,used:512,files:[]});
        if(url==='/evt/days'||url.startsWith('/evt?')||url==='/evt')return response(200,'');
        if(!owns)return response(403,{ok:0,e:'admin'});
        if(url==='/evt/format'){formatted=true;return response(200,'ok');}
        return response(200,{ok:1});
    };
    w.fetch=server;w.confirm=()=>true;
    d.getElementById('admPass').value='admin';
    await w.uiAction(login,()=>w.admLogin(),true);
    check(w.sessionStorage.getItem('co_admin')===token,'login stores only the session token for this tab');
    const formatIcon=d.getElementById('evtFormat');
    check(!formatIcon.hidden && formatIcon.closest('.icog') && formatIcon.querySelector('use').getAttribute('href')==='#i-trash','format icon returns alongside memory tools after login');
    await w.uiAction(formatIcon,()=>w.evtfmt(),true);
    check(formatted && !d.getElementById('adminCard').hidden && !formatIcon.hidden,'cookie-blocked format succeeds without throwing admin out');
    check(w.sessionStorage.getItem('co_admin')===token,'format preserves the same session token');
    await w.uiAction(formatIcon,()=>w.admPost('/evt/clr',{},'پاک شد'),true);
    await w.uiAction(formatIcon,()=>w.admPost('/clear',{},'پاک شد'),true);
    check(!d.getElementById('adminCard').hidden,'both clearing actions preserve cookie-blocked admin session');
    await w.uiAction(login,()=>w.admLogin(),true);
    check(d.getElementById('admGate').hidden,'existing owner can reauthenticate without active-session error');
    const reloaded=new JSDOM(html,{runScripts:'outside-only',url:'http://panel.test/'});
    const rw=reloaded.window;
    rw.sessionStorage.setItem('co_admin',token);rw.fetch=server;rw.setInterval=()=>0;rw.addEventListener=()=>{};
    rw.eval(html.match(/<script>([\s\S]*?)<\/script>/)[1]);
    rw.admProbe();await tick();
    check(!rw.document.getElementById('adminCard').hidden && !rw.document.getElementById('evtFormat').hidden,'reload restores owner through saved tab token, without cookies');
    rw.close();
    w.fetch=async()=>{throw Error('temporary network loss');};
    await w.uiAction(formatIcon,()=>w.evtfmt(),true);
    check(!d.getElementById('adminCard').hidden && w.sessionStorage.getItem('co_admin')===token,'network failure during format does not discard admin session');
    check(d.getElementById('admCardMsg').textContent.includes('نامشخص'),'lost format response is unknown, not a false failure or success');
    for(const [body,word] of [['mount','باز نشد'],['write','تأیید نشد']]){
        w.fetch=(u,o)=>u==='/evt/format'?Promise.resolve(response(500,body)):server(u,o);
        await w.uiAction(formatIcon,()=>w.evtfmt(),true);
        check(!d.getElementById('adminCard').hidden && d.getElementById('admCardMsg').textContent.includes(word),'specific '+body+' failure stays visible without logout');
    }
    let storageStatus={fs:0,mountFail:1,total:0,used:0};
    w.fetch=(u,o)=>u==='/evt/fs'?Promise.resolve(response(200,storageStatus)):server(u,o);
    await w.uiAction(formatIcon,()=>w.evtfmt(),true);
    check(!d.getElementById('admCardMsg').textContent.includes('رمزها حفظ شدند') && d.getElementById('admCardMsg').textContent.includes('در دسترس نیست'),'format does not claim final storage health when readback is unavailable');
    storageStatus={fs:1,total:65536,used:512,files:[]};
    await w.uiAction(formatIcon,()=>w.evtfmt(),true);
    check(d.getElementById('admCardMsg').textContent.includes('نوشتن و خواندن تأیید شد'),'success includes verified format and fresh mounted storage status');
    const header=d.querySelector('#evtFs > div');
    check(header && header.querySelector('b') && !header.querySelector('div'),'storage header markup closes before the detailed status');
    const unusual='/evil"<img src=x onerror=alert(1)>.bin';
    let disk=[{name:'/users.json',size:180},{name:'/wifi.json',size:120},{name:unusual,size:7}],deletes=[],deleteStatus=200;
    let more=true;
    w.fetch=async(u,o={})=>{
        if(u.startsWith('/evt/fs'))return response(200,{fs:1,total:65536,used:307,files:disk,count:disk.length,more:more?2:0,next:more?32:-1});
        if(u==='/evt/file'){
            const name=new URLSearchParams(o.body).get('name');deletes.push([name,o]);
            if(deleteStatus!==200)return response(deleteStatus,{ok:0,e:deleteStatus===404?'missing':'remove'});
            disk=disk.filter(f=>f.name!==name);return response(200,{ok:1});
        }
        return server(u,o);
    };
    await w.evtFs(0);
    let remove=[...d.querySelectorAll('#evtFs [data-file]')];
    check(remove.length===3,'every listed file, including settings, has an admin delete button');
    check(remove[2].dataset.file===unusual && !d.querySelector('#evtFs img'),'hostile filename stays exact in dataset and cannot inject HTML');
    for(const b of d.querySelectorAll('#evtFs [onclick]'))w.Function(b.getAttribute('onclick'));
    check(true,'dynamic delete and pagination handlers compile');
    const next=[...d.querySelectorAll('#evtFs button')].find(b=>b.getAttribute('onclick').includes('evtFs(0,32)'));
    check(!!next,'large storage listings offer access to remaining files');
    w.confirm=()=>false;await w.uiAction(remove[0],()=>w.evtDelete('/users.json'),true);
    check(!deletes.length && disk.length===3,'cancel never sends a delete request');
    let warning='';w.confirm=t=>{warning=t;return true;};
    await w.uiAction(remove[0],()=>w.evtDelete('/users.json'),true);
    check(warning.includes('رمز مدیر پیش‌فرض'),'deleting account file warns about the reboot/default-password consequence');
    check(deletes.length===1 && deletes[0][0]==='/users.json' && deletes[0][1].headers['X-Admin-Token']===token,'delete sends the exact selected path with the admin token');
    check(!disk.some(f=>f.name==='/users.json') && disk.some(f=>f.name==='/wifi.json') && !d.getElementById('adminCard').hidden,'one-file deletion preserves unrelated files and the owner session');
    deleteStatus=500;await w.evtDelete('/wifi.json');
    check(d.getElementById('toastMsg').textContent.includes('انجام نشد') && disk.length===2,'flash remove error is visible and never claims success');
    deleteStatus=404;await w.evtDelete('/wifi.json');
    check(d.getElementById('toastMsg').textContent.includes('وجود ندارد'),'already removed file is reported separately');
    const before=deletes.length;
    w.fetch=()=>Promise.reject(Error('offline'));await w.evtDelete('/wifi.json');
    check(deletes.length===before && w.sessionStorage.getItem('co_admin')===token && d.getElementById('toastMsg').textContent.includes('پاسخ حذف نرسید'),'lost delete response keeps session and asks for a fresh list');
    let resolveList;
    w.fetch=()=>new Promise(r=>resolveList=r);
    const pending=w.evtFs(0);w.admLock();resolveList(response(200,{fs:1,total:65536,files:disk}));await pending;
    check(!d.getElementById('evtFs').textContent && d.getElementById('evtFs').style.display==='none','late admin file-list response cannot expose settings after logout');
    let unsignedCalls=0;w.fetch=()=>{unsignedCalls++;throw Error('must not send');};await w.evtDelete('/wifi.json');
    check(!unsignedCalls && !d.getElementById('admGate').hidden,'unsigned delete opens the gate without sending any mutation');
    w.admTokenSet(token);w.admUnlock('admin');
    w.fetch=server;await w.uiAction(d.querySelector('#adminCard button'),()=>w.admLogout(),true);
    check(!active && !w.sessionStorage.getItem('co_admin') && formatIcon.hidden,'explicit logout invalidates session and hides format icon');
    console.log(`ALL ${checks} PANEL ACTION CHECKS PASSED`);
})().then(()=>{dom.window.close();}).catch(error=>{console.error(error);dom.window.close();process.exitCode=1;});
