/* [EN] Real panel bench workflow; only hardware/flash boundaries are simulated. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom not installed; bench-repeat DOM tests did not run.');process.exit(0);}
let checks=0;
function check(ok,why){assert.ok(ok,why);++checks;console.log('  ok   '+why);}
const pause=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
 const intervals=[];
 const dom=new JSDOM(fs.readFileSync(path.join(__dirname,'../panel_preview.html'),'utf8'),{
  runScripts:'dangerously',pretendToBeVisual:true,url:'http://localhost/',
  beforeParse(w){const set=w.setInterval.bind(w);w.setInterval=(f,t)=>{const id=set(f,t);intervals.push(id);return id;};}
 });
 const w=dom.window,d=w.document;
 try{
 await pause(80);intervals.splice(0).forEach(id=>w.clearInterval(id));
 const state=()=>w.eval('W'),samples=()=>w.eval('CALS');
 const wait=async(pred,why)=>{const end=Date.now()+2500;while(!pred()){if(Date.now()>end)throw Error('timeout: '+why+' / '+d.getElementById('wS0').textContent);await pause(5);}};
 const form=async(x,after=0)=>wait(()=>state().current===x&&state().choose&&d.getElementById('wGo')&&windows>after,'form '+x);
 w.D={on:1,fl:0,seq:1,p:Array(171).fill(0),t:Array(30).fill(0)};
 w.D.p[13]=w.D.p[14]=500;w.D.t[17]=w.D.t[18]=12000;
 const original=[11,12,16,18,19].map(id=>w.D.p[id]);
 w.eval('CALS=[];CALP=[];CALR=[];');
 w.pmConfirm=async()=>true;w.pmAlert=async text=>{throw Error('unexpected alert: '+text);};
 w.winfo=async()=>({fs:1,size:0});
 const writes=[],commands=[];let windows=0,failWrite=false;
 w.setv=async(id,v)=>{commands.push([id,v]);w.D.p[id]=v;if(id===19)w.D.fl=v?(w.D.fl|32):(w.D.fl&~32);};
 w.wopen=async()=>{++windows;state().winMs=Date.now();};
 w.wlatch=async()=>({n:1,seq:windows,fl:32,or:0,lo:Array(30).fill(100),hi:Array(30).fill(100),la:Array(30).fill(100),a:()=>windows*100});
 w.wlog=async text=>{if(failWrite&&text[0]!=='#')throw 'simulated flash error';writes.push(text);};
 const fields=()=>{const a=state().act;for(const n of a)d.getElementById('wB'+n).value=String(100+n);};
 const submit=()=>{fields();d.getElementById('wGo').click();};
 const active=()=>[11,12,16,18,19].map(id=>w.D.p[id]);
 d.getElementById('wSw').checked=false;d.getElementById('wL').value='5,10';
 ['BAT1','BAT2','BOTH'].forEach(sc=>d.getElementById('wc'+sc).checked=sc!=='BOTH');
 const run=w.wStart();await form(0);
 check(!d.getElementById('wIi')&&!d.getElementById('wT').textContent.includes('جریان ورودی'),'capture asks only for active battery currents, with no input-current field or table column');
 const report=w.pmAlert,emptyAlerts=[];w.pmAlert=async msg=>emptyAlerts.push(msg);d.getElementById('wGo').click();
 check(emptyAlerts.length===1&&emptyAlerts[0].includes('جریان باتری 1')&&samples().length===0,'removing input-current collection does not weaken mandatory active-battery current validation');w.pmAlert=report;
 check(w.getComputedStyle(d.getElementById('wGo')).pointerEvents!=='none','active capture submit is not disabled by the start-button run lock');
 check(w.wlive([1]).length===2&&d.getElementById('wr0').children[3].textContent==='0','live row updates match the two board-current columns without overwriting DMM cells');
 check(d.querySelectorAll('[data-wretry]').length===4,'each selected scenario/step has its own repeat control');
 check(d.querySelector('[data-wretry="1"]').disabled,'unmeasured future steps cannot be accidentally energized as repeats');
 const beforeCurrent=windows;d.querySelector('[data-wretry="0"]').click();await form(0,beforeCurrent);
 check(windows===beforeCurrent+1&&samples().length===0,'repeating current row resets the measurement window without committing');
 submit();
 check([...d.querySelectorAll('[data-wretry]')].every(b=>b.disabled),'repeat controls lock while a measurement is being committed');
 d.getElementById('wGo').click();await form(1);
 check(samples().length===1,'saving a repeated current row advances exactly once');
 const capturedRow=writes.find(t=>t.startsWith('BAT1,')),cols=capturedRow.trimEnd().split(',');
 check(cols.length===53&&cols[49]==='-'&&cols[50]==='101'&&cols[51]==='-','battery-only capture keeps the 53-column CSV aligned and marks legacy input current missing, not zero');
 check(state().K[0].cells.length===4&&d.getElementById('wr0').children.length===9&&d.getElementById('wr0').children[7].textContent==='ثبت شد'&&d.querySelector('[data-wretry="0"]'),'removing the input column preserves current values, status and repeat-button alignment');
 d.getElementById('wB1').value='-25';d.getElementById('wN').value='draft note';
 const win=windows;d.querySelector('[data-wretry="0"]').click();await form(0,win);
 check(state().act.join(',')==='1'&&state().next===1,'a completed earlier step can be revisited without advancing the pending step');
 w.eval('CALR=[[1]];CALP=[[1]];');const oldRaw=samples()[0].r1;
 submit();await form(1);
 check(samples().length===1&&samples()[0].r1!==oldRaw,'successful recapture replaces just that step instead of double-weighting it');
 check(w.eval('CALR.length===0&&CALP.length===0'),'recapture invalidates stale computed calibration results');
 check(d.getElementById('wB1').value==='-25'&&d.getElementById('wN').value==='draft note','unsaved DMM input for the interrupted step is restored');
 check(writes.some(t=>t.startsWith('# repeat scenario=BAT1 step=1')),'append-only CSV keeps the prior attempt and explicitly marks the repeat');
 submit();await form(2);const second=samples()[1],oldSamples=samples().slice();
 const cross=windows;d.querySelector('[data-wretry="0"]').click();await form(0,cross);
 check(state().act.join(',')==='1'&&w.D.p[11]===1&&w.D.p[12]===0,'cross-scenario repeat safely selects the original channel configuration');
 submit();await form(2);
 check(state().act.join(',')==='2'&&w.D.p[11]===0&&w.D.p[12]===1,'after recapture the interrupted scenario and step resume');
 check(samples().length===2&&samples()[1]===second,'other completed measurements remain unchanged across scenario switches');
 submit();await form(3);submit();
 await wait(()=>d.getElementById('wReviewEnd'),'final review');
 check(state().run&&samples().length===4,'last measurement opens review rather than finishing before the user can repeat');
 check(JSON.stringify(active())===JSON.stringify(original)&&!state().man,'original board settings are restored before waiting in review');
 check([...d.querySelectorAll('[data-wretry]')].every(b=>!b.disabled),'every measured row remains repeatable before final confirmation');
 const rw=windows;d.querySelector('[data-wretry="1"]').click();await form(1,rw);
 const first=samples()[0],third=samples()[2];submit();await wait(()=>d.getElementById('wReviewEnd'),'review after repeat');
 check(samples().length===4&&samples()[0]===first&&samples()[2]===third&&samples()[1]!==second,'review recapture replaces only its own sample and returns to review');
 d.getElementById('wReviewEnd').click();await run;
 check(!state().run&&JSON.stringify(active())===JSON.stringify(original),'final confirmation completes once and leaves original settings restored');
 check([...d.querySelectorAll('[data-wretry]')].every(b=>!b.disabled),'finished-session rows remain available for explicit recapture');
 const savedTable=state().K,finishedSamples=samples().slice(),finishedWrites=writes.length;
 w.pmConfirm=async()=>false;await w.wrepeat(1);
 check(!state().run&&writes.length===finishedWrites&&state().K===savedTable,'canceling post-finish recapture preserves rows and sends no hardware or file writes');
 const alerts=[];w.pmAlert=async msg=>alerts.push(msg);w.D.on=0;await w.wrepeat(1);w.D.on=1;
 check(alerts.length===1&&!state().run&&writes.length===finishedWrites,'post-finish recapture requires a live link before any write');
 w.D.p[15]=1;w.pmConfirm=async()=>true;await w.wrepeat(1);w.D.p[15]=0;
 check(alerts.length===2&&!state().run,'post-finish recapture refuses fixed-duty mode');
 const info=w.winfo;w.winfo=async()=>({fs:0});await w.wrepeat(1);w.winfo=info;
 check(alerts.length===3&&!state().run,'post-finish recapture refuses unavailable storage without altering samples');
 w.pmConfirm=async()=>{w.D.on=0;return true;};await w.wrepeat(1);w.D.on=1;
 check(alerts.length===4&&!state().run&&!state().opening&&writes.length===finishedWrites,'link loss during confirmation is rechecked before touching the hardware');
 w.pmConfirm=async()=>{w.eval('CAL_BUSY=true');return true;};await w.wrepeat(1);w.eval('CAL_BUSY=false');
 check(alerts.length===5&&!state().run&&!state().opening,'a calibration transaction starting during confirmation prevents recapture');

 w.pmAlert=async text=>{throw Error('unexpected alert: '+text);};
 let confirmCount=0,accept;w.pmConfirm=()=>{++confirmCount;return new Promise(r=>accept=r);};
 const reopened=w.wrepeat(1);await wait(()=>accept,'repeat confirmation');w.wrepeat(2);
 check(confirmCount===1&&state().opening&&!state().run,'double-clicks cannot open two post-finish capture sessions');
 w.D.p[11]=1;w.D.p[16]=30;const nowOriginal=active();accept(true);w.pmConfirm=async()=>true;
 const reopenWin=windows;await form(1,reopenWin);
 check(state().K===savedTable&&state().retry===1&&state().next===4,'finished-session recapture reuses original steps and does not restart the sweep');
 submit();await wait(()=>d.getElementById('wReviewEnd'),'review after reopening');
 check(samples().length===finishedSamples.length&&samples()[1]!==finishedSamples[1]&&samples()[0]===finishedSamples[0]&&samples()[2]===finishedSamples[2],'post-finish capture replaces only the selected sample');
 check(JSON.stringify(active())===JSON.stringify(nowOriginal),'post-finish capture restores settings as they were immediately before the new capture');
 d.getElementById('wReviewEnd').click();await reopened;
 check(!state().run&&!state().opening&&[...d.querySelectorAll('[data-wretry]')].every(b=>!b.disabled),'after another finish, rows are still repeatable and confirmation lock is released');
 [11,12,16,18,19].forEach((id,i)=>w.D.p[id]=original[i]);w.D.fl=0;


 /* [EN] Failed repeated writes must preserve the previously committed measurement. */
 d.getElementById('wL').value='5';d.getElementById('wcBAT2').checked=false;
 const run2=w.wStart();await form(0);submit();await wait(()=>d.getElementById('wReviewEnd'),'second review');
 let prior=JSON.stringify(samples());const prev=windows;
 d.querySelector('[data-wretry="0"]').click();await form(0,prev);failWrite=true;submit();await run2;failWrite=false;
 check(JSON.stringify(samples())===prior,'failed CSV write never replaces a previously successful sample');
 check(!state().run&&JSON.stringify(active())===JSON.stringify(original),'write failure exits safely and restores channel/duty/manual state');
 check(d.getElementById('wr0').dataset.calissue==='error','a failed repeated write marks its exact bench row for attention');
 const repairWin=windows,repair=w.wrepeat(0);await form(0,repairWin);submit();await wait(()=>d.getElementById('wReviewEnd'),'repaired failure review');
 check(!state().K[0].issue&&d.getElementById('wr0').dataset.calissue!=='error','successful recapture immediately clears the obsolete failed-step marker');
 d.getElementById('wReviewEnd').click();await repair;prior=JSON.stringify(samples());


 const run3=w.wStart();await form(0);w.D.on=0;await run3;w.D.on=1;
 check(!state().run&&!d.getElementById('wGo')&&JSON.stringify(active())===JSON.stringify(original),'link loss while entering DMM data stops capture and cleans up the form');
 check(JSON.stringify(samples())===prior,'link loss does not add or overwrite a measurement');

 const run4=w.wStart();await form(0);d.getElementById('wEn').click();await run4;
 check(!state().run&&JSON.stringify(active())===JSON.stringify(original),'early End retains the existing safe restore path');

 const realSet=w.setv;let prepFailure=true;const beforePrep=windows;
 w.setv=async(id,v)=>{if(prepFailure&&id===18){prepFailure=false;throw 'prepare failure';}return realSet(id,v);};
 await w.wStart();w.setv=realSet;
 check(windows===beforePrep&&!state().run&&JSON.stringify(active())===JSON.stringify(original),'preparation failure restores settings before any capture window opens');
 const run5=w.wStart();await form(0);submit();await wait(()=>d.getElementById('wReviewEnd'),'reorder review');
 const replaced=state().K[0].savedSample;const other=samples()[0];const last=windows;
 d.querySelector('[data-wretry="0"]').click();await form(0,last);
 w.eval('CALS=CALS.slice().reverse()');submit();await wait(()=>d.getElementById('wReviewEnd'),'reorder retry');
 check(!samples().includes(replaced)&&samples().includes(other),'repeat replacement follows sample identity even if the sample list was reordered');
 state().abort=true;await run5;
 check(!state().run&&!state().choose&&!d.getElementById('wReviewEnd'),'global End also exits review and cleans up its pending choice');
 console.log('ALL '+checks+' BENCH REPEAT TESTS PASSED');
 }finally{dom.window.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
