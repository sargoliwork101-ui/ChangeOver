/* [EN] Real panel: named per-variable send progress, readback and pending reports. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom missing; send-feedback tests did not run.');process.exit(0);}
let checks=0,failures=0;
function check(ok,msg){++checks;try{assert.ok(ok,msg);console.log('  ok   '+msg);}catch(e){++failures;console.error('  FAIL '+msg);}}
const pause=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
 const html=fs.readFileSync(process.env.PANEL_FEEDBACK_BASELINE||path.join(__dirname,'../panel_preview.html'),'utf8');
 const dom=new JSDOM(html,{url:'http://localhost/',runScripts:'dangerously',pretendToBeVisual:true,beforeParse(w){w.scrollTo=()=>{};w.setInterval=()=>0;const t=w.setTimeout.bind(w);w.setTimeout=(fn,ms,...a)=>fn&&fn.name==='poll'?0:t(fn,ms,...a);}});
 const w=dom.window,d=w.document;
 try{
  await pause(80);
  const reset=()=>{w.eval('PEND={};CAL_BUSY=false;W.run=W.opening=false;');w.D={on:1,seq:10,p:Array(171).fill(0),t:Array(31).fill(0)};};
  const text=()=>d.getElementById('srsm').textContent;
  reset();
  // Real asynchronous send: hold the first POST while checking its progress.
  let release;const posts=[];
  w.fetch=async url=>{const u=new URL(url,'http://localhost/'),id=+u.searchParams.get('id'),v=+u.searchParams.get('v');posts.push([id,v]);if(posts.length===1)await new Promise(r=>{release=r;});w.D.p[id]=v;++w.D.seq;return {ok:true};};
  w.qput(20,14400);w.qput(25,650);const sending=w.sendall();
  check(d.getElementById('sbst').textContent.includes('حداکثر ولتاژ باتری (Absorb)'),'live progress names the variable currently being sent');
  check(d.getElementById('sbst').textContent.includes('جریان Bulk'),'live progress names the variable waiting its turn');
  release();await sending;
  check(text().includes('حداکثر ولتاژ باتری (Absorb)')&&text().includes('جریان Bulk'),'successful batch feedback names every sent variable');
  check((text().match(/به‌روزرسانی تأیید شد/g)||[]).length===2,'successful batch gives each variable its own confirmed status');
  check(!/\b(?:20|25):|شناسهٔ/.test(text()),'feedback uses names, not bare parameter indices');
  check(JSON.stringify(posts)===JSON.stringify([[20,14400],[25,650]]),'display changes leave the actual protocol ids and values intact');
  check(w.eval('!Object.keys(PEND).length&&!CAL_BUSY'),'normal acknowledgement reconciliation and hardware ownership remain intact');
  // Mixed real result: acknowledged, HTTP-failed, missing readback and clamped.
  reset();w.D.p[20]=14400;w.D.p[21]=14300;w.D.p[23]=null;w.D.p[24]=12500;
  w.fetch=async url=>{const id=+new URL(url,'http://localhost').searchParams.get('id');++w.D.seq;return {ok:id!==21};};
  w.qput(20,14400);w.qput(21,14300);w.qput(23,13500);w.qput(24,12800);await w.sendall();
  const rows=[...d.querySelectorAll('#srsm li')].map(e=>e.textContent);
  const has=(name,status)=>rows.some(r=>r.startsWith(name+' — ')&&r.includes(status));
  check(has('حداکثر ولتاژ باتری (Absorb)','به‌روزرسانی تأیید شد'),'mixed result identifies the acknowledged variable by name');
  check(has('آستانهٔ ورود به Absorb','ارسال تأیید نشد'),'mixed result identifies the failed-send variable by name');
  check(has('ولتاژ Float','تأیید برد نرسید'),'missing readback is attached to its variable name');
  check(has('ولتاژ بازگشت به Bulk','بازخورد متفاوت'),'clamped readback is distinct from a confirmed requested update');
  check(has('ولتاژ بازگشت به Bulk','مقدار برد: 12500'),'different applied board value is visible next to the variable name');
  check(w.eval('PEND[21]===14300&&PEND[23]===13500&&!(20 in PEND)&&!(24 in PEND)'),'mixed reporting preserves the existing queue reconciliation rules');
  check(d.getElementById('srst').textContent.includes('تأییدشده: 1 از 4'),'result counts confirmed requested values, not merely HTTP successes');
  check(d.getElementById('srsa').textContent.includes('دوباره بفرست'),'remaining pending variables retain an explicit retry action');
  // The renderer also covers paths that intentionally have no trustworthy echo.
  check(typeof w.sendresult==='function','one feedback renderer serves success, failure and absent echo paths');
  if(typeof w.sendresult==='function'){
   reset();w.qput(20,14400);w.D.p[20]=14400;w.sendresult({20:14400},{20:14400},false);
   check(text().includes('حداکثر ولتاژ باتری (Absorb)')&&text().includes('تأیید برد نرسید'),'no-echo result names the variable and does not use a stale matching value as confirmation');
   check(!text().includes('به‌روزرسانی تأیید شد'),'no-echo report never claims a confirmed update');
   w.sendresult({20:14400},{},false);
   check(text().includes('ارسال تأیید نشد')&&text().includes('هنوز در صف'),'total transport failure lists the named pending variable');
   w.qput(20,14500);w.qput(25,700);w.sendresult({20:14400},{20:14400},true);
   check(text().includes('حداکثر ولتاژ باتری (Absorb) — به‌روزرسانی تأیید شد؛ ویرایش تازه در صف: 14500'),'a newer edit is shown under its name separately from its old confirmed value');
   check(text().includes('جریان Bulk — هنوز ارسال نشده؛ در صف: 700'),'newly queued variables not in the batch are named as unsent');
   check(w.eval('PEND[20]===14500&&PEND[25]===700'),'report generation itself never writes, clears or modifies pending values');
  }
  check(typeof w.pname==='function','a common canonical-name resolver is present');
  if(typeof w.pname==='function'){
   check(w.eval('XIDS.every(id=>pname(id)&&!/^شناسه|^پارامتر ناشناخته/.test(pname(id)))'),'every declared backup/send variable resolves to a name');
   check(w.pname(13)!==w.pname(14),'channel-specific duty variable names remain distinguishable');
   reset();w.qput(20,17000);w.qput(25,650);let calls=0;w.fetch=async()=>{++calls;return {ok:true};};await w.sendall();
   check(calls===0&&text().includes('حداکثر ولتاژ باتری (Absorb)')&&!text().includes('شناسهٔ'),'out-of-range rejection uses the variable name, not an index');
   check(text().includes('جریان Bulk'),'preflight rejection also names other changes left waiting in the queue');
   w.lnkhealth({nk:1,nid:25,nr:2});
   check(d.getElementById('lnkw').textContent.includes('جریان Bulk')&&!d.getElementById('lnkw').textContent.includes('شناسهٔ'),'board rejection notice resolves the rejected parameter name');
   w.eval('PEND={};');w.qput(2,1100);check(w.calpendingText([2]).includes('ضریب جریان ۱')&&!w.calpendingText([2]).includes('شناسهٔ'),'calibration conflicts name the pending variable');
   const label=d.createElement('label'),input=d.createElement('input');input.id='q999';label.append(d.createTextNode('<img src=x onerror=bad()>'),input);d.body.append(label);
   w.sendresult({999:1},{},false);
   check(!d.querySelector('#srsm img')&&text().includes('<img src=x onerror=bad()>'),'names inserted into result HTML are escaped as text');
  }
  console.log(checks+' SEND FEEDBACK CHECKS; '+failures+' FAILURE(S)');if(failures)process.exitCode=1;
 }finally{dom.window.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
