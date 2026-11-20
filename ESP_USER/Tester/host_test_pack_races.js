'use strict';
// Late replies must not restore private data, old public status or false save success.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const {JSDOM,VirtualConsole}=require('jsdom');
const html=fs.readFileSync(path.join(__dirname,'../simple_preview.html'),'utf8');
let checks=0;
const tick=async()=>{for(let i=0;i<12;i++)await new Promise(resolve=>setImmediate(resolve));};
function check(ok,why){assert.ok(ok,why);checks++;}
const response=(status,data)=>({status,ok:status===200,json:async()=>data});
async function session(){
 const errors=[],intervals=[],timeouts=new Map(),vc=new VirtualConsole();let timerId=100000;
 vc.on('jsdomError',e=>errors.push(e.message));
 const dom=new JSDOM(html,{url:'file:///panel.html',runScripts:'dangerously',pretendToBeVisual:true,virtualConsole:vc,beforeParse(w){
  w.HTMLCanvasElement.prototype.getContext=()=>null;w.setInterval=(fn,ms)=>{intervals.push([fn,ms]);return 0;};
  const original=w.setTimeout.bind(w),clear=w.clearTimeout.bind(w);
  w.setTimeout=(fn,ms,...args)=>{if(ms===8000){const id=++timerId;timeouts.set(id,fn);return id;}return original(fn,ms,...args);};
  w.clearTimeout=id=>{if(!timeouts.delete(id))clear(id);};
 }});
 await tick();const w=dom.window,d=w.document,fetch=w.fetch;
 async function login(){w.admOpen();d.getElementById('admPass').value='admin';d.getElementById('admLoginBtn').click();await tick();}
 async function save(value){d.getElementById('wearLimit').value=value;d.getElementById('previewLimitForm').dispatchEvent(new w.Event('submit',{cancelable:true,bubbles:true}));await tick();}
 const poll=intervals.find(([fn,ms])=>ms===5000&&fn.name==='poll')[0];
 return {w,d,fetch,login,save,poll,timeouts,close(){check(!errors.length,'no script errors in isolated race case');w.close();}};
}
(async()=>{
 // A private GET that finishes after logout cannot refill hidden metadata.
 {
  const p=await session();let resolve;
  p.w.fetch=(url,opts)=>url==='/pack/settings'?new Promise(r=>resolve=r):p.fetch(url,opts);
  await p.login();await p.w.admLogout();resolve(response(200,{ok:1,limit:999,at:1793836800,revision:4}));await tick();
  check(p.d.getElementById('adminCard').hidden&&p.d.getElementById('previewSavedLimit').textContent==='—'&&p.d.getElementById('previewReplacedAt').textContent==='—','late private GET discarded after logout');p.close();
 }
 // A committed POST can arrive late without restoring the departed session or success message.
 {
  const p=await session();await p.login();let resolve;
  p.w.fetch=(url,opts)=>url==='/pack/settings'&&opts?.method==='POST'?new Promise(r=>resolve=r):p.fetch(url,opts);
  await p.save('300');check(p.d.getElementById('previewLimitSave').disabled,'save locked in flight');
  await p.w.admLogout();resolve(response(200,{ok:1,limit:300,at:0,revision:1}));await tick();
  check(p.d.getElementById('previewSavedLimit').textContent==='—'&&!p.d.getElementById('previewLimitStatus').textContent&&p.d.getElementById('adminCard').hidden,'late POST cannot repopulate private data or success after logout');p.close();
 }
 // A public poll started before mutation cannot overwrite the post-save result.
 {
  const p=await session();await p.login();let resolve,hold=true;
  p.w.fetch=(url,opts)=>url==='/pack'&&hold?(hold=false,new Promise(r=>resolve=r)):p.fetch(url,opts);
  const pending=p.poll();await tick();await p.save('300');
  const state=p.d.querySelector('.wear-state').textContent;
  resolve(response(200,{valid:1,state:5,count:99999,progress:1000,replaced:0}));await pending;
  check(p.d.querySelector('.wear-number').textContent!=='۹۹٬۹۹۹'&&p.d.querySelector('.wear-state').textContent===state,'stale pre-save poll cannot overwrite new result');p.close();
 }
 for(const action of ['read','save','replace']){
  const p=await session();if(action!=='read')await p.login();
  p.w.fetch=(url,opts)=>(url==='/pack/settings'&&(action==='read'||opts?.method==='POST'))||url==='/pack/replace'?Promise.resolve(response(403,{ok:0,e:'admin'})):p.fetch(url,opts);
  if(action==='read')await p.login();else if(action==='save')await p.save('300');else{p.w.confirm=()=>true;p.d.getElementById('previewReplaceBattery').click();await tick();}
  check(p.d.getElementById('adminCard').hidden&&p.d.getElementById('packLogSnapshot').hidden,'server 403 locks the panel for '+action);p.close();
 }
 for(const [error,expected] of [['data','اطلاعات تازه'],['revision','اطلاعات تغییر کرده'],['record','نیاز به بررسی'],['storage','انجام نشد']]){
  const p=await session();await p.login();let posts=0;
  p.w.fetch=(url,opts)=>url==='/pack/replace'?(posts++,Promise.resolve(response(409,{ok:0,e:error}))):p.fetch(url,opts);
  p.w.confirm=()=>true;p.d.getElementById('previewReplaceBattery').click();await tick();
  check(posts===1&&p.d.getElementById('previewLimitStatus').textContent.includes(expected)&&!p.d.getElementById('previewReplaceBattery').disabled,'mutation error is truthful and unlocks controls: '+error);p.close();
 }
 {
  const p=await session();await p.login();await p.save('300');
  p.d.getElementById('previewLimitClear').click();await tick();
  check(p.d.getElementById('previewSavedLimit').textContent==='تنظیم نشده'&&p.d.querySelector('.wear-dot').hidden,'explicit disable removes indicator without invented default');
  let reject;
  p.w.fetch=(url,opts)=>url==='/pack'?new Promise((resolve,r)=>{reject=r;opts.signal.addEventListener('abort',()=>r(Error('aborted')));}):p.fetch(url,opts);
  const pending=p.poll();await tick();check(p.timeouts.size===1,'public poll has an eight-second deadline');
  for(const callback of [...p.timeouts.values()])callback();await pending;
  check(p.d.querySelector('.wear-number').textContent==='—'&&p.d.querySelector('.wear-dot').hidden,'aborted poll neutralizes stale result');check(typeof reject==='function','timeout exercised real abort listener');p.close();
 }
 {
  const p=await session();let posts=0;
  p.w.fetch=(url,opts)=>url==='/pack/settings'?(opts?.method==='POST'&&posts++,Promise.reject(Error('read failed'))):p.fetch(url,opts);
  await p.login();await p.save('300');check(!posts&&p.d.getElementById('previewLimitStatus').textContent.includes('خوانده نشد'),'failed settings read cannot submit guessed revision');p.close();
 }
 console.log('ALL '+checks+' PACK RACE / FAILURE CHECKS PASSED');
})().catch(e=>{console.error(e);process.exit(1);});
