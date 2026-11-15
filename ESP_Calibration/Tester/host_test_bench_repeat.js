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
 const fields=()=>{const a=state().act;for(const n of a)d.getElementById('wB'+n).value=String(100+n);d.getElementById('wIi').value='150';};
 const submit=()=>{fields();d.getElementById('wGo').click();};
 const active=()=>[11,12,16,18,19].map(id=>w.D.p[id]);
 d.getElementById('wSw').checked=false;d.getElementById('wL').value='5,10';
 ['BAT1','BAT2','BOTH'].forEach(sc=>d.getElementById('wc'+sc).checked=sc!=='BOTH');
 const run=w.wStart();await form(0);
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
 d.getElementById('wB1').value='-25';d.getElementById('wIi').value='175';
 const win=windows;d.querySelector('[data-wretry="0"]').click();await form(0,win);
 check(state().act.join(',')==='1'&&state().next===1,'a completed earlier step can be revisited without advancing the pending step');
 w.eval('CALR=[[1]];CALP=[[1]];');const oldRaw=samples()[0].r1;
 submit();await form(1);
 check(samples().length===1&&samples()[0].r1!==oldRaw,'successful recapture replaces just that step instead of double-weighting it');
 check(w.eval('CALR.length===0&&CALP.length===0'),'recapture invalidates stale computed calibration results');
 check(d.getElementById('wB1').value==='-25'&&d.getElementById('wIi').value==='175','unsaved DMM input for the interrupted step is restored');
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
 check([...d.querySelectorAll('[data-wretry]')].every(b=>b.disabled),'closed sessions cannot accidentally command the hardware through old rows');

 /* [EN] Failed repeated writes must preserve the previously committed measurement. */
 d.getElementById('wL').value='5';d.getElementById('wcBAT2').checked=false;
 const run2=w.wStart();await form(0);submit();await wait(()=>d.getElementById('wReviewEnd'),'second review');
 const prior=JSON.stringify(samples());const prev=windows;
 d.querySelector('[data-wretry="0"]').click();await form(0,prev);failWrite=true;submit();await run2;failWrite=false;
 check(JSON.stringify(samples())===prior,'failed CSV write never replaces a previously successful sample');
 check(!state().run&&JSON.stringify(active())===JSON.stringify(original),'write failure exits safely and restores channel/duty/manual state');

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
