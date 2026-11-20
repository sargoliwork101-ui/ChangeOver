'use strict';
// Exercise the shipped shared UI against the generated mock board and authenticated native request helpers.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const {JSDOM,VirtualConsole}=require('jsdom');
const root=path.join(__dirname,'..'),html=fs.readFileSync(path.join(root,'simple_preview.html'),'utf8');
let checks=0;
function check(ok,label){assert.ok(ok,label);checks++;console.log('PASS: '+label);}
const tick=async()=>{for(let i=0;i<12;i++)await new Promise(r=>setImmediate(r));};
(async()=>{
 const errors=[],vc=new VirtualConsole();vc.on('jsdomError',e=>errors.push(e.message));const timers=[];
 const dom=new JSDOM(html,{url:'file:///panel.html',runScripts:'dangerously',pretendToBeVisual:true,virtualConsole:vc,beforeParse(w){w.setInterval=(fn,ms)=>{timers.push([fn,ms]);return 0;};w.HTMLCanvasElement.prototype.getContext=()=>null;}});
 const w=dom.window,d=w.document;await tick();
 const card=d.getElementById('packWork'),settings=d.getElementById('packReviewSettings');
 check(card&&card.previousElementSibling.contains(d.getElementById('stUptime'))&&card.nextElementSibling.querySelector('#cCharge'),'one production pack card remains between statistics and calendar');
 check(d.querySelectorAll('#packWork').length===1&&!card.querySelector('input,button,details'),'public production card is read-only');
 check(!d.querySelector('#packReviewBanner,#wearUpper,#wearLower,#wearHasData,.preview-admin-demo'),'shipped UI contains no preview banner or mock inputs');
 const shared=fs.readFileSync(path.join(root,'pack_review_panel.h'),'utf8');check(!/localStorage|STORAGE_KEY|Date.now\(\)/.test(shared),'production maintenance uses no browser persistence or browser replacement date');
 check(d.getElementById('adminCard').hidden&&d.getElementById('packLogSnapshot').hidden,'private settings and relocated snapshot start locked');
 check(card.querySelector('.wear-state').textContent==='یادآور بررسی تنظیم نشده','first use has no invented service threshold');
 const original=w.fetch,requests=[];w.fetch=async(url,opts)=>{requests.push([url,opts]);return original(url,opts);};
 const poll=timers.find(([fn,ms])=>ms===5000&&fn.name==='poll')[0];
 w.admOpen();d.getElementById('admPass').value='admin';d.getElementById('admLoginBtn').click();await tick();
 check(!d.getElementById('adminCard').hidden&&!d.getElementById('packLogSnapshot').hidden,'real login unlocks maintenance and snapshot');
 check(requests.some(([url,opts])=>url==='/pack/settings'&&opts.headers['X-Admin-Token']),'private settings request carries the real admin token');
 check(d.querySelectorAll('#adminCard>.preview-admin-group').length===3&&d.querySelectorAll('#adminCard .admin-section').length===6,'all six admin sections retain approved grouping');
 check(!d.getElementById('evtFormat')&&d.getElementById('admFormat')&&!d.querySelector('[onclick*="admDl()"]'),'duplicate download and format panels are removed');
 check(d.getElementById('packLogSnapshot').closest('#evtCard'),'manual snapshot stays beside logs');
 async function save(value){d.getElementById('wearLimit').value=value;d.getElementById('previewLimitForm').dispatchEvent(new w.Event('submit',{bubbles:true,cancelable:true}));await tick();}
 await save('300');check(d.getElementById('previewSavedLimit').textContent==='۳۰۰ شارژ کامل'&&d.getElementById('previewLimitStatus').textContent==='در برد ذخیره شد.','successful ESP response updates threshold with board-save wording');
 const before=await(await original('/diag')).json(),log=await(await original('/evt')).text();
 let confirms=0;w.confirm=()=>{confirms++;return false;};const replace=d.getElementById('previewReplaceBattery');replace.click();await tick();
 check(confirms===1&&!requests.some(([url])=>url==='/pack/replace'),'cancel sends no replacement request');
 let release;w.fetch=(url,opts)=>url==='/pack/replace'?new Promise(resolve=>{release=()=>original(url,opts).then(resolve);}):original(url,opts);
 w.confirm=()=>{confirms++;return true;};replace.click();replace.click();await tick();
 check(replace.disabled&&confirms===2,'replacement stays disabled while save is pending and double click asks once');
 check(!d.getElementById('previewLimitStatus').textContent.includes('تعویض باتری در برد ذخیره شد'),'pending save never claims success');
 release();await tick();check(card.querySelector('.wear-number').textContent==='۰'&&card.querySelector('.wear-unit').textContent==='شارژ پس از تعویض','confirmed response resets displayed pack work');
 const date=d.getElementById('previewReplacedAt').textContent;check(date!=='ثبت نشده'&&date!=='—'&&d.getElementById('previewLimitStatus').textContent.includes('در برد ذخیره شد'),'confirmed replacement displays returned date and board-save wording');
 const after=await(await original('/diag')).json();check(after.chargeCountCh1===before.chargeCountCh1&&after.chargeCountCh2===before.chargeCountCh2&&JSON.stringify(after.chargeHistory)===JSON.stringify(before.chargeHistory)&&await(await original('/evt')).text()===log,'maintenance leaves lifetime counters and history unchanged');
 w.fetch=original;await save('500');check(d.getElementById('previewReplacedAt').textContent===date&&card.querySelector('.wear-number').textContent==='۰','threshold editing preserves replacement date and baseline');
 for(const value of ['',0,-1,1.5,100001]){await save(value);check(d.getElementById('previewSavedLimit').textContent==='۵۰۰ شارژ کامل','invalid input preserves saved threshold: '+value);}
 w.fetch=async(url,opts)=>url==='/pack/settings'&&opts&&opts.method==='POST'?{status:503,ok:false,json:async()=>({ok:0,e:'storage'})}:original(url,opts);
 await save('900');check(d.getElementById('previewSavedLimit').textContent==='۵۰۰ شارژ کامل'&&d.getElementById('previewLimitStatus').textContent.includes('انجام نشد'),'storage failure never claims board save or changes active threshold');
 w.fetch=async(url,opts)=>url==='/pack/replace'?Promise.reject(Error('lost response')):original(url,opts);replace.click();await tick();check(d.getElementById('previewLimitStatus').textContent.includes('نامشخص'),'lost reply is explicitly unknown, not a false failure or success');
 w.fetch=original;await w.admLogout();await tick();check(d.getElementById('adminCard').hidden&&d.getElementById('packLogSnapshot').hidden&&d.getElementById('previewReplacedAt').textContent==='—'&&d.getElementById('wearLimit').value==='','logout clears private metadata and unsaved edits');
 await poll();check(card.querySelector('.wear-number').textContent==='۰','public pack work remains readable without administrator login');
 for(const [state,count,message] of [[0,0,'در انتظار دریافت اطلاعات'],[1,0,'اطلاعات کارکرد نیاز به بررسی دارد'],[3,1,'در محدودهٔ کارکرد تنظیم‌شده'],[4,400,'نزدیک زمان بررسی'],[5,500,'زمان بررسی پک رسیده است']]){
  w.fetch=async()=>({ok:true,json:async()=>({valid:state>=3?1:0,state,count,progress:state===4?800:state===5?1000:2,replaced:1})});await poll();
  check(card.querySelector('.wear-state').textContent===message&&card.querySelector('.wear-dot').hidden===(state<3),'ESP-derived status rendered faithfully: '+state);
 }
 w.fetch=async()=>{throw Error('disconnected');};await poll();check(card.querySelector('.wear-number').textContent==='—'&&card.querySelector('.wear-dot').hidden,'network loss neutralizes stale public result');
 check(!errors.length,'production shared UI has no script errors');w.close();
 console.log('ALL '+checks+' PRODUCTION PACK DOM CHECKS PASSED');
})().catch(e=>{console.error(e);process.exit(1);});
