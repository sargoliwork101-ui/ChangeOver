/* [EN] Real-panel regression: transaction ownership, validated send snapshots and safe reconciliation. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom missing; transaction audit did not run.');process.exit(0);}
const page=fs.readFileSync(process.env.PANEL_TX_BASELINE||path.join(__dirname,'../panel_preview.html'),'utf8');
let checks=0,failures=0;
function check(ok,msg){++checks;try{assert.ok(ok,msg);console.log('  ok   '+msg);}catch(e){++failures;console.error('  FAIL '+msg);}}
const pause=ms=>new Promise(r=>setTimeout(r,ms));
const watchdog=setTimeout(()=>{console.error('FAIL: transaction audit timed out');process.exit(1);},90000);
const table={ready:true,T:[{X:[80,100],Y:[0,30]},{X:[],Y:[]}]};
const proposal={...table,target:1,body:'test',crc:123,msg:[],cal:{ids:[0,2],offset:[0,0],gain:[1000,1000],voltage:[0,0,0],selected:{0:true,2:true}}};
async function scenario(test){
 const timers=[];
 const dom=new JSDOM(page,{runScripts:'dangerously',pretendToBeVisual:true,url:'http://localhost/',beforeParse(w){w.scrollTo=()=>{};const timeout=w.setTimeout.bind(w);w.setTimeout=(fn,ms,...args)=>fn&&fn.name==='poll'?0:timeout(fn,ms,...args);const set=w.setInterval.bind(w);w.setInterval=(f,t)=>{const id=set(f,t);timers.push(id);return id;};}});
 const w=dom.window,d=w.document;
 try{
  await pause(50);timers.forEach(id=>w.clearInterval(id));
  w.D={on:1,seq:10,fl:0,p:Array(171).fill(0),t:Array(31).fill(0)};w.D.p[2]=w.D.p[3]=1000;
  w.eval('PEND={};W={run:false,opening:false};CAL_BUSY=false;');
  w.lview=()=>{};w.pmConfirm=async()=>false;
  w.lread=async()=>JSON.parse(JSON.stringify(table));w.lcalread=async()=>true;
  await test(w,d);
 }finally{dom.window.close();}
}
(async()=>{
 for(const state of ['run','opening']){
  await scenario(async(w,d)=>{
   let posts=0;w.fetch=async()=>{++posts;return {ok:true};};w.qput(16,100);w.eval('W.'+state+'=true');
   await w.sendall();
   check(posts===0&&w.eval('PEND[16]===100'),'global parameter send is blocked during capture '+state+' and retains pending edits');
   let commits=0;w.lcommit=async()=>{++commits;};w.eval('LUT_PENDING='+JSON.stringify({target:1,before:table,p:proposal,selected:{}}));
   await w.lapplypending();
   check(commits===0&&w.eval('!!LUT_PENDING'),'pending LUT application cannot overtake capture '+state);
   w.eval('LUT_PENDING=null;LSNT=1;');let reads=0;w.lread=async()=>{++reads;return table;};await w.lreadnow();
   check(reads===0&&w.eval('LSNT===1'),'manual table read cannot disturb capture '+state+' or consume reset eligibility');
   reads=0;w.eval('LUT_PENDING=null;');w.lpack=()=>JSON.parse(JSON.stringify(proposal));await w.lsend(1);
   check(reads===0&&!w.eval('LUT_PENDING'),'table proposal preparation is blocked during capture '+state);
   let dialogs=0;w.calmodalOpen=()=>{++dialogs;};await w.calapply();
   check(dialogs===0,'calibration apply dialog is blocked during capture '+state);
   let backups=0;w.caljobs=()=>[[2,1100,'gain',1000]];w.xexp=async()=>{++backups;return false;};await w.calmodalApply();
   check(backups===0,'confirmed calibration application is blocked during capture '+state);
   let voltageDialogs=0;w.pmConfirm=async()=>{++voltageDialogs;return false;};d.getElementById('vm0').value='13';await w.vcal(0);
   check(voltageDialogs===0,'voltage calibration is blocked during capture '+state);
  });
 }
 await scenario(async(w)=>{
  let release,posts=0,starts=0;
  w.fetch=()=>{++posts;if(posts>1)return Promise.resolve({ok:true});return new Promise(res=>{release=()=>res({ok:true});});};
  w.wbegin=async()=>{++starts;};w.qput(16,100);
  const sending=w.sendall();
  check(w.eval('CAL_BUSY'),'global send owns the hardware before its first asynchronous POST');
  await w.wStart();await w.sendall();
  check(starts===0&&posts===1,'capture and duplicate send cannot start while global send is awaiting its reply');
  release();await sending;
  check(!w.eval('CAL_BUSY'),'global send releases ownership on completion');
 });
 for(const accepted of [true,false]){
  await scenario(async(w,d)=>{
   let decide,writes=0,starts=0;d.getElementById('vm0').value='13';
   w.pmConfirm=()=>new Promise(res=>{decide=res;});w.setv=async()=>{++writes;};w.wbegin=async()=>{++starts;};
   const op=w.vcal(0);
   check(w.eval('CAL_BUSY'),'voltage calibration owns hardware while confirmation is pending ('+accepted+')');
   await w.wStart();check(starts===0,'capture cannot start under a pending voltage confirmation ('+accepted+')');
   decide(accepted);await op;
   check(writes===(accepted?1:0)&&!w.eval('CAL_BUSY'),'voltage confirmation '+accepted+' writes only when accepted and releases ownership');
  });
 }
 for(const state of ['run','opening','busy']){
  await scenario(async(w,d)=>{
   w.eval('LSNT=1;LUT_LAST_PROPOSAL='+JSON.stringify(proposal)+';'+(state==='busy'?'CAL_BUSY=true;':'W.'+state+'=true;'));
   let resets=0;w.req=async()=>{++resets;return {ok:1,_s:200};};w.lupd();
   check(d.getElementById('lbtnR').disabled,'reset button is disabled while '+state+' owns hardware');
   await w.lrst();check(resets===0&&w.eval('LSNT===1'),'reset handler rejects '+state+' without posting or consuming the verified proposal');
  });
 }
 await scenario(async(w)=>{
  w.eval('LSNT=1;LUT_LAST_PROPOSAL='+JSON.stringify(proposal));let release,resets=0,starts=0;
  w.req=()=>{++resets;if(resets>1)return Promise.resolve({ok:1,_s:200});return new Promise(res=>{release=()=>res({ok:1,_s:200});});};w.wbegin=async()=>{++starts;};
  const op=w.lrst();check(w.eval('CAL_BUSY&&LREAD_IN_PROGRESS'),'reset owns hardware before awaiting its first POST');
  await w.wStart();await w.lrst();check(starts===0&&resets===1,'reset cannot race capture or issue a duplicate reset while POST is pending');
  release();await op;check(!w.eval('CAL_BUSY||LREAD_IN_PROGRESS'),'reset releases ownership after readback');
 });
 await scenario(async(w)=>{
  w.eval('LSNT=1;LUT_LAST_PROPOSAL='+JSON.stringify(proposal));w.req=async()=>({ok:1,_s:200});w.lread=async()=>{throw Error('injected readback failure');};
  try{await w.lrst();}catch(e){}
  check(!w.eval('CAL_BUSY||LREAD_IN_PROGRESS'),'unexpected reset readback failure cannot strand the hardware lock');
 });
 await scenario(async(w)=>{
  w.eval('CAL_BUSY=true;W.run=true;');const calls=[];w.fetch=async(u)=>{calls.push(u);return {ok:true};};
  w.send(19,0);w.send(11,0);await Promise.resolve();
  check(calls.some(u=>u==='/s?id=19&v=0')&&calls.some(u=>u==='/s?id=11&v=0'),'manual-mode exit and emergency charger-off commands remain usable while hardware is owned');
 });
 await scenario(async(w)=>{
  w.qput(16,100);w.fetch=async()=>{throw Error('network failure');};await w.sendall();
  check(!w.eval('CAL_BUSY')&&w.eval('PEND[16]===100'),'failed global send releases ownership and retains its pending value');
 });
 await scenario(async(w,d)=>{
  d.getElementById('vm0').value='13';w.pmConfirm=async()=>{throw Error('dialog failure');};try{await w.vcal(0);}catch(e){}
  check(!w.eval('CAL_BUSY'),'voltage confirmation exception releases ownership');
 });
 await scenario(async(w)=>{
  let posts=0;w.req=async()=>{++posts;return {ok:1};};await w.lrst();
  check(posts===0&&!w.eval('CAL_BUSY||LREAD_IN_PROGRESS'),'reset without verified eligibility sends nothing and releases ownership');
 });
 await scenario(async(w)=>{
  w.eval('LUT_PENDING='+JSON.stringify({target:1,before:table,p:proposal,selected:{}}));let resets=0;
  w.lapplycal=async()=>{};w.pmConfirm=async()=>true;
  w.req=async(u,m)=>{if(u==='/lut/reset'){++resets;return {ok:1,_s:200};}return m==='POST'?{ok:1,_s:200}:{st:3,s:0,crc:123,_s:200};};
  await w.lapplypending();
  check(resets===1&&w.eval('LSNT===1&&!CAL_BUSY&&!LREAD_IN_PROGRESS'),'successful commit retains its authorized automatic reset and final exact readback');
 });
 /* [EN] A send owns a validated batch, not the mutable editors used for the next batch. */
 await scenario(async(w,d)=>{
  const posts=[];let release;
  w.fetch=async u=>{const url=new URL(u,'http://localhost'),id=+url.searchParams.get('id'),v=+url.searchParams.get('v');
   posts.push([id,v]);if(posts.length===1)await new Promise(r=>{release=r;});w.D.p[id]=v;++w.D.seq;return {ok:true};};
  w.qput(20,14400);w.qput(21,14300);const op=w.sendall();
  check(posts.length===1&&posts[0][0]===20,'validated batch begins with the first approved value');
  w.qput(21,17000);w.qput(25,700);release();await op;
  check(JSON.stringify(posts)===JSON.stringify([[20,14400],[21,14300]]),'later edits cannot replace validated values or add unapproved writes to the in-flight batch');
  check(w.D.p[21]===14300,'an out-of-range edit made during POST is not sent past the completed range check');
  check(w.eval('PEND[21]===17000&&PEND[25]===700'),'readback preserves newer edits and newly queued ids for a separate send');
  check(w.eval('evp(21).v===17000&&evp(21).p'),'the chart retains its newer unsent value and preview affordance');
  check(w.eval('!(20 in PEND)&&!CAL_BUSY'),'acknowledged unchanged pending entry is reconciled and ownership is released');
  const count=posts.length;await w.sendall();
  check(posts.length===count,'the next batch validates the newer out-of-range value and sends nothing');
 });
 await scenario(async(w,d)=>{
  const posts=[];let release;
  w.fetch=async u=>{const url=new URL(u,'http://localhost'),id=+url.searchParams.get('id'),v=+url.searchParams.get('v');
   posts.push([id,v]);if(posts.length===1)await new Promise(r=>{release=r;});w.D.p[id]=v;++w.D.seq;return {ok:true};};
  w.qput(27,14400);const op=w.sendall();w.qput(27,14500);release();await op;
  check(w.eval('PEND[27]===14500'),'editing an already-posted id while awaiting its acknowledgement does not lose the new edit');
  check(d.getElementById('q27').value==='14500','old readback cannot overwrite a newer visible value');
  check(w.D.p[27]===14400&&posts.length===1,'newly typed value is not silently written by the old batch');
  await w.sendall();
  check(JSON.stringify(posts)===JSON.stringify([[27,14400],[27,14500]]),'a subsequent explicit send submits the retained new value');
  check(w.eval('!(27 in PEND)&&!CAL_BUSY')&&d.getElementById('q27').value==='14500','second acknowledgement reconciles its own batch normally');
 });
 await scenario(async(w)=>{
  let posts=0;w.fetch=async()=>{++posts;return {ok:true};};
  const ids=w.eval('Object.keys(EVB).map(Number)');
  check(ids.every(id=>{const m=w.pmeta(id),b=w.eval('EVB['+id+']');return m&&m.lo===b[0]&&m.hi===b[1];}),'every chart and chip parameter has pre-send metadata matching its editor limits');
  check(ids.every(id=>{w.eval('PEND={}');w.qput(id,w.eval('EVB['+id+'][1]')+1);const high=w.pvalidate([id]).length===1;w.qput(id,w.eval('EVB['+id+'][0]')-1);return high&&w.pvalidate([id]).length===1;}),'all chart and chip parameters reject below-limit and above-limit pending values');
  w.eval('PEND={}');w.qput(20,17000);await w.sendall();
  check(posts===0&&w.eval('PEND[20]===17000'),'out-of-range graph-only edit is blocked before the first POST and retained for correction');
 });
 console.log(checks+' TRANSACTION AUDIT CHECKS; '+failures+' FAILURE(S)');
 clearTimeout(watchdog);if(failures)process.exitCode=1;
})().catch(e=>{clearTimeout(watchdog);console.error(e);process.exitCode=1;});
