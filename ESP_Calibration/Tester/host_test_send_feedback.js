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
  check((text().match(/مقدار برد مطابق درخواست است/g)||[]).length===2,'successful batch gives each variable its own confirmed status');
  check(!/\b(?:20|25):|شناسهٔ/.test(text()),'feedback uses names, not bare parameter indices');
  check(JSON.stringify(posts)===JSON.stringify([[20,14400],[25,650]]),'display changes leave the actual protocol ids and values intact');
  check(w.eval('!Object.keys(PEND).length&&!CAL_BUSY'),'normal acknowledgement reconciliation and hardware ownership remain intact');
  // Mixed real result: acknowledged, HTTP-failed, missing readback and clamped.
  reset();w.D.p[20]=14400;w.D.p[21]=14300;w.D.p[23]=null;w.D.p[24]=12500;
  w.fetch=async url=>{const id=+new URL(url,'http://localhost').searchParams.get('id');++w.D.seq;return {ok:id!==21};};
  w.qput(20,14400);w.qput(21,14300);w.qput(23,13500);w.qput(24,12800);await w.sendall();
  const rows=[...d.querySelectorAll('#srsm li')].map(e=>e.textContent);
  const has=(name,status)=>rows.some(r=>r.startsWith(name)&&r.includes(status));
  check(has('حداکثر ولتاژ باتری (Absorb)','مقدار برد مطابق درخواست است'),'mixed result identifies the acknowledged variable by name');
  check(has('آستانهٔ ورود به Absorb','ارسال تأیید نشده؛ نتیجه معلوم نیست'),'mixed result identifies the failed-send variable by name');
  check(has('ولتاژ Float','پاسخ برد نرسیده؛ نتیجه معلوم نیست'),'missing readback is attached to its variable name');
  check(has('ولتاژ بازگشت به Bulk','مقدار برد با درخواست فرق دارد'),'clamped readback is distinct from a confirmed requested update');
  check(has('ولتاژ بازگشت به Bulk','آخرین مقدار برد: 12500'),'different applied board value is visible next to the variable name');
  check(w.eval('PEND[21]===14300&&PEND[23]===13500&&!(20 in PEND)&&PEND[24]===12800'),'only matching readbacks leave the queue; mismatches stay retryable');
  check(d.getElementById('srst').textContent.includes('تأییدشده: 1 از 4'),'result counts confirmed requested values, not merely HTTP successes');
  check(d.getElementById('srsa').textContent.includes('ارسال دوبارهٔ موارد تأییدنشده'),'remaining pending variables retain an explicit retry action');
  // The renderer also covers paths that intentionally have no trustworthy echo.
  check(typeof w.sendresult==='function','one feedback renderer serves success, failure and absent echo paths');
  if(typeof w.sendresult==='function'){
   reset();w.qput(20,14400);w.D.p[20]=14400;w.sendresult({20:14400},{20:14400},false);
   check(text().includes('حداکثر ولتاژ باتری (Absorb)')&&text().includes('پاسخ برد نرسیده؛ نتیجه معلوم نیست'),'no-echo result names the variable and does not use a stale matching value as confirmation');
   check(!text().includes('مقدار برد مطابق درخواست است'),'no-echo report never claims a confirmed update');
   w.sendresult({20:14400},{},false);
   check(text().includes('ارسال تأیید نشده؛ نتیجه معلوم نیست')&&text().includes('منتظر روشن‌شدن نتیجه'),'total transport failure lists the named pending variable');
   w.qput(20,14500);w.qput(25,700);w.sendresult({20:14400},{20:14400},true);
   check(d.querySelector('[data-id="20"]').textContent.includes('ویرایش تازه در صف: 14500'),'a newer edit is shown under its name separately from its old confirmed value');
   check(d.querySelector('[data-id="25"]').textContent.includes('هنوز به برد فرستاده نشده'),'newly queued variables not in the batch are named as unsent');
   check(w.eval('PEND[20]===14500&&PEND[25]===700'),'report generation itself never writes, clears or modifies pending values');
  }
  // Real report actions: retained mismatches, delayed replies, selective retries and locks.
  reset();w.D.p[24]=12500;w.qput(24,12800);
  w.fetch=async()=>{++w.D.seq;return {ok:true};};await w.sendall();
  check(w.eval('PEND[24]===12800')&&d.querySelector('#srsa button[onclick="sendRetry()"]'),'a mismatched-only batch retains a retry button and its requested value');
  let reads=0;w.fetch=async(u,o)=>{check(u==='/t'&&o.cache==='no-store','recheck reads telemetry without a write');reads++;return {ok:true,json:async()=>({...w.D,seq:99,p:w.D.p.map((v,i)=>i===24?12800:v)})};};
  await w.sendRecheck();
  check(reads===1&&w.eval('!(24 in PEND)&&!CAL_BUSY'),'delayed matching readback clears only the original pending value and releases lock');
  check(d.querySelector('.srgroup details')===null&&text().includes('مقدار برد مطابق درخواست است')&&!d.querySelector('#srsa [onclick="sendRetry()"]'),'recheck refreshes report and removes obsolete retry action');
  reset();w.qput(24,12800);w.sendresult({24:12800},{24:12800},false);
  w.fetch=async()=>({ok:true,json:async()=>({...w.D,p:w.D.p.map((v,i)=>i===24?12800:v)})});await w.sendRecheck();
  check(w.eval('PEND[24]===12800')&&d.getElementById('srnotice').textContent.includes('حفظ شد'),'same-sequence response never confirms stale values or destroys prior report');
  w.fetch=async()=>{throw Error('offline')};await w.sendRecheck();
  check(w.eval('PEND[24]===12800&&!CAL_BUSY'),'network failure preserves request and unlocks recheck');
  w.qput(24,12900);w.fetch=async()=>({ok:true,json:async()=>({...w.D,seq:99,p:w.D.p.map((v,i)=>i===24?12800:v)})});await w.sendRecheck();
  check(w.eval('PEND[24]===12900'),'recheck cannot consume a newer edit');
  reset();w.D.p[20]=14400;w.qput(24,12800);w.qput(25,650);w.sendresult({20:14400,24:12800},{20:14400,24:12800},true);
  const retried=[];w.fetch=async u=>{const url=new URL(u,'http://localhost'),id=+url.searchParams.get('id'),v=+url.searchParams.get('v');retried.push([id,v]);w.D.p[id]=v;w.D.seq++;return {ok:true};};await w.sendRetry();
  check(JSON.stringify(retried)==='[[24,12800]]'&&w.eval('PEND[25]===650'),'retry sends unresolved original entry only, never successes or unrelated edits');
  reset();w.qput(24,12800);w.sendresult({24:12800},{},false);w.qput(24,12900);let writes=0;w.fetch=async()=>{writes++;return {ok:true}};await w.sendRetry();
  check(writes===0&&w.eval('PEND[24]===12900'),'retry cannot silently send a newer edit');
  for(const state of ['CAL_BUSY=true','W.run=true','W.opening=true']){
   reset();w.qput(24,12800);w.sendresult({24:12800},{},false);w.eval(state);await w.sendRecheck();await w.sendRetry();
   check(writes===0,'report actions respect hardware ownership: '+state);
  }
  reset();w.qput(24,12800);w.sendresult({24:12800},{},false);w.pundo();await w.sendRetry();
  check(writes===0,'cancelled edits are never resurrected by old report retry');
  reset();w.D.p[20]=14400;w.D.p[24]=12500;
  w.qput(24,12800);w.qput(25,650);w.sendresult({20:14400,24:12800,25:650},{20:14400,24:12800},true);
  const count=k=>+d.querySelector('[data-filter="'+k+'"] strong').textContent;
  check(count('confirmed')===1&&count('different')===1&&count('unknown')===1,'summary shows separate accurate counts, not transport success counts');
  check(!d.querySelector('[data-group="different"]').hidden&&d.querySelector('[data-group="confirmed"]').hidden,'report opens on differences, not a long list of successes');
  let reportWrites=0;w.fetch=async()=>{reportWrites++;return {ok:true}};
  d.querySelector('[data-filter="confirmed"]').click();
  check(!d.querySelector('[data-group="confirmed"]').hidden&&d.querySelector('[data-group="different"]').hidden,'confirmed filter shows only the settings actually matching the board');
  check(d.querySelector('[data-filter="confirmed"]').getAttribute('aria-pressed')==='true','selected summary button exposes its state to assistive technology');
  d.querySelector('[data-filter="unknown"]').click();
  check(!d.querySelector('[data-group="unknown"]').hidden&&d.querySelector('[data-group="unknown"]').textContent.includes('جریان Bulk'),'unknown filter names the specific unconfirmed setting');
  check(reportWrites===0&&w.eval('PEND[24]===12800&&PEND[25]===650'),'switching report filters cannot write or change pending edits');
  reset();w.D.p[20]=14400;w.sendresult({20:14400},{20:14400},true);
  check(!d.querySelector('[data-group="confirmed"]').hidden&&count('different')===0&&count('unknown')===0,'all-success report opens directly on successful rows');
  reset();
  const batch={},sent={},sampleRows=[],retryOnly=[24,25,26,27,28,29];
  [20,21,23,24,25,26,27,28,29].forEach((id,i)=>{batch[id]=({20:14400,21:14300,23:13500,24:12800,25:650,26:100,27:14800,28:150,29:6000})[id];sent[id]=batch[id];w.D.p[id]=i<3?batch[id]:i<6?batch[id]+1:null;sampleRows.push({id:String(id),back:w.D.p[id]});if(i>=3)w.qput(id,batch[id]);});
  [30,31,32].forEach(id=>w.qput(id,w.eval('pdflt('+id+')')??0));
  w.sendresult(batch,sent,true,[],sampleRows);
  check(['confirmed','different','unknown','queued'].every(k=>count(k)===3&&d.querySelectorAll('[data-group="'+k+'"] .sritem').length===3),'real report presents three distinct settings per category, including unsent edits');
  check(d.querySelector('[data-group="confirmed"] p').textContent.includes('ماندگاری پس از خاموشی را ثابت نمی‌کند'),'matching values do not claim power-cycle persistence');
  check(d.querySelector('[data-group="different"] p').textContent.includes('ابتدا «بررسی دوبارهٔ نتیجه»'),'mismatch explanation gives the read-only next step');
  check(d.querySelector('[data-group="unknown"] p').textContent.includes('نه اینکه حتماً اعمال نشده'),'missing feedback is explicitly not proof of failure');
  check(d.querySelector('[data-group="queued"] p').textContent.includes('ارسال مجدد شامل آن‌ها نیست'),'unsent settings explain the separate send-all action');
  check(d.querySelector('[onclick="sendRetry()"]').textContent==='ارسال دوبارهٔ موارد تأییدنشده'&&d.querySelector('[onclick="sendRecheck()"]').textContent==='بررسی دوبارهٔ نتیجه','production action buttons use the approved plain-language labels');
  const fixturePosts=[];w.fetch=async u=>{const url=new URL(u,'http://localhost'),id=+url.searchParams.get('id'),v=+url.searchParams.get('v');fixturePosts.push(id);w.D.p[id]=v;++w.D.seq;return {ok:true};};
  await w.sendRetry();
  check(JSON.stringify(fixturePosts)===JSON.stringify(retryOnly),'real retry posts only the six unresolved original values, not three matches or three unsent edits');
  check(w.eval('Object.keys(PEND).sort().join()')==='30,31,32'&&!d.querySelector('[onclick="sendRetry()"]'),'after successful retry only unrelated unsent edits remain and retry disappears');
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
