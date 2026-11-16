/* [EN] Restore audit: real panel imports, queue reconciliation, modal choices and safe hardware teardown. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom missing; restore-audit DOM tests did not run.');process.exit(0);}
const timers=[];let checks=0;
function check(ok,msg){assert.ok(ok,msg);++checks;console.log('  ok   '+msg);}
const pause=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
 const dom=new JSDOM(fs.readFileSync(process.env.PANEL_RESTORE_BASELINE||path.join(__dirname,'../panel_preview.html'),'utf8'),{runScripts:'dangerously',pretendToBeVisual:true,url:'http://localhost/',beforeParse(w){w.scrollTo=()=>{};w.localStorage.setItem('changeover-panel-pending-v1','{"20":14500}');const set=w.setInterval.bind(w);w.setInterval=(f,t)=>{const id=set(f,t);timers.push(id);return id;};}});
 const w=dom.window,d=w.document;
 try{
 await pause(80);timers.forEach(id=>w.clearInterval(id));
 const wait=async(pred,label)=>{const end=Date.now()+3000;while(!pred()){if(Date.now()>end)throw Error('timeout '+label);await pause(5);}};
 w.D={on:1,seq:12,fl:7,p:w.eval('Array.from({length:171},(_,i)=>pdflt(i)??0)'),t:Array(31).fill(0)};w.D.p[13]=w.D.p[14]=500;w.D.t[17]=w.D.t[18]=12000;
 w.fetch=async()=>({ok:true,json:async()=>w.D,text:async()=>''});
 let commands=0;w.setv=async()=>{++commands;};
 const snapshot=()=>w.eval('JSON.stringify({CALS,CALCUT,PEND})');
 const file=o=>({text:async()=>JSON.stringify(o)});
 const settings=fields=>({app:'ChangeOver-settings',v:4,schema:w.xschema(),...fields});
 w.pmConfirm=async()=>true;
 await w.ximp(file(settings({params:{4:null,5:'',6:false,7:'10',8:true}})));
 check(w.eval('Object.keys(PEND).length')===0&&commands===0,'null, empty, boolean and numeric-text settings never turn into staged numeric values');
 check(w.localStorage.getItem('changeover-panel-pending-v1')===null&&typeof w.pendRestore==='undefined'&&typeof w.pendSave==='undefined','broken automatic pending-queue recovery and its obsolete storage key are completely removed');
 w.qput(83,14500);w.qput(5,123);await w.ximp(file(settings({params:{83:w.D.p[83]}})));
 check(w.eval('PEND[83]===undefined&&PEND[5]===123'),'an imported board-equal value cancels its stale pending edit while unrelated edits survive');
 check(!d.getElementById('q83').classList.contains('pq')&&+d.getElementById('q83').value===w.D.p[83],'visible controls agree with reconciled pending state immediately');
 w.qput(4,234);await w.ximp(file(settings({params:{4:0},pending:{4:null}})));
 check(w.eval('PEND[4]===undefined'),'numeric zero remains valid while a null pending entry does not override it');
 await w.ximp(file(settings({pending:{'083':14000,6:'12',7:false,8:null,70:28000}})));
 check(w.eval('PEND[83]===undefined&&PEND[6]===undefined&&PEND[7]===undefined&&PEND[8]===undefined&&PEND[70]===undefined'),'malformed pending IDs/values and derived id 70 cannot create phantom changes');
 const unchanged=snapshot();w.pmConfirm=async()=>false;await w.ximp(file(settings({params:{83:14200}})));
 check(snapshot()===unchanged,'canceling settings import changes neither samples nor pending values');w.pmConfirm=async()=>true;
 await w.ximp(file(settings({params:{83:14200},benchSamples:[{r1:null,sc:'BOTH'}]})));
 check(snapshot()===unchanged,'a damaged calibration sample rejects the entire backup before staging any setting');
 for(const raw of [null,[],false,'bad']){await w.ximp(file(settings({params:raw})));check(snapshot()===unchanged,'invalid settings structure cannot mutate the current session');}
 await w.ximp(file(settings({lut:{ready:1,T:[{X:[1,2],Y:[0,20]},{X:[1,2],Y:[0,20]}]}})));
 check(typeof w.LUT_BACKUP==='undefined'&&typeof w.xlutimport==='undefined'&&d.getElementById('xst').textContent.includes('بازیابی مستقیم'),'dead imported-LUT storage is gone and LUT-only files are explicitly reported as unsupported');
 let exported;const save=w.filesave;w.filesave=async(name,data)=>{exported=JSON.parse(data);return true;};w.eval('LBT={ready:true,T:[{X:[80,100],Y:[0,20]},{X:[],Y:[]}]}');
 await w.xexp();w.filesave=save;
 check(exported.lutRole==='audit-only'&&exported.lut.T[0].X[0]===80,'exported LUT snapshot is explicitly an audit record, not a false restore promise');
 const seed=()=>{w.eval('CALS=[];W={run:false};');for(let i=0;i<4;i++)w.CALS.push({sc:'BOTH',d:i*5,use:1,r1:80+i*20,r2:90+i*20,b1:i?i*30:-20,b2:i?i*31:-15,ts:100+i});w.calinvalidate();w.calsmp();w.calrun();};
 seed();const archive={app:'ChangeOver-bench-samples',v:2,view:{cuts:[[120],[]],scenarios:['BOTH']},samples:JSON.parse(JSON.stringify(w.CALS))};archive.samples[3].b1=91.125;
 const before=snapshot();w.eval('W.K=[]');w.wbuild(['BOTH'],[0]);w.eval('W.K[0].done=true');const rowBefore=w.eval('W.K');
 let op=w.calimp(file(archive));await wait(()=>d.getElementById('pmappend'),'import choice');d.getElementById('pmno').click();await op;
 check(snapshot()===before&&w.eval('W.K')===rowBefore,'sample-import Cancel preserves the exact current dataset, selections and capture rows');
 check(!d.getElementById('pmappend')&&d.getElementById('pmok').textContent==='تأیید','three-way import controls are cleaned up before the next dialog');
 op=w.calimp(file(archive));await wait(()=>d.getElementById('pmappend'),'escape import');d.dispatchEvent(new w.KeyboardEvent('keydown',{key:'Escape',bubbles:true}));await op;
 check(snapshot()===before,'Escape during sample import is non-destructive');
 op=w.calimp(file(archive));await wait(()=>d.getElementById('pmappend'),'replace import');d.getElementById('pmok').click();await op;
 check(w.CALS.length===4&&w.CALS[3].b1===91.125&&!w.calbuild(1).X.includes(120)&&w.eval('CALR.length')>0,'explicit replacement restores the saved fractional readings, point choices and recalculated table');
 check(w.eval('W.K.length')===0&&!d.getElementById('wT').textContent,'replacing the dataset removes stale capture rows instead of allowing wrong-sample recapture');
 const first=w.CALS[0];w.wbuild(['BOTH'],[0]);w.eval('W.K[0].savedSample=CALS[0]');
 op=w.calimp(file(archive));await wait(()=>d.getElementById('pmappend'),'append import');d.getElementById('pmappend').click();await op;
 check(w.CALS.length===8&&w.CALS[0]===first&&w.eval('W.K[0].savedSample')===first,'explicit append remains functional and preserves existing capture-sample links');
 const dataBefore=snapshot();w.eval('W.opening=true');await w.calimp(file(archive));await w.ximp(file(settings({params:{83:14200}})));w.eval('W.opening=false');
 check(snapshot()===dataBefore,'capture preflight owns the data until its confirmation completes');
 w.pmConfirm=async()=>{w.eval('W.run=true');return true;};await w.calclr();w.eval('W.run=false');
 check(snapshot()===dataBefore,'sample-clear rechecks capture ownership after its asynchronous confirmation');
 let clearPosts=0;w.req=async()=>{++clearPosts;return {_s:200};};await w.wclear();w.eval('W.run=false');
 check(clearPosts===0,'file-clear also rechecks capture ownership before deleting the bench log');w.pmConfirm=async()=>true;
 const realImport=w.calimp,realSettings=w.ximp;let inputValue;
 w.calimp=async()=>inputValue=d.getElementById('calf').value;Object.defineProperty(d.getElementById('calf'),'files',{configurable:true,value:[file(archive)]});Object.defineProperty(d.getElementById('calf'),'value',{configurable:true,writable:true,value:'old.json'});d.getElementById('calf').dispatchEvent(new w.Event('change'));
 check(inputValue==='','sample file input resets before parsing so the same file can be selected again after cancel or error');w.calimp=realImport;
 w.ximp=async()=>inputValue=d.getElementById('xim').value;Object.defineProperty(d.getElementById('xim'),'files',{configurable:true,value:[file(settings({}))]});Object.defineProperty(d.getElementById('xim'),'value',{configurable:true,writable:true,value:'old.json'});d.getElementById('xim').dispatchEvent(new w.Event('change'));
 check(inputValue==='','settings file input also supports reselecting the same file after cancel or error');w.ximp=realSettings;
 const writes=[];w.setv=async(id,v)=>{writes.push([id,v]);if(id===18)throw 'no echo';};w.eval('W={run:false}');const restoreResult=await w.wrestore({11:0,12:0,16:20,18:30,19:0});
 check(typeof restoreResult==='string'&&restoreResult.includes('مقدار duty ثابت ۲')&&!restoreResult.includes('شناسه‌های')&&writes.length===7,'hardware restore failure is returned while best-effort restoration of remaining settings continues');
 w.setv=async()=>{};check(await w.wrestore({11:0,12:0,16:0,18:0,19:0})===null,'successful restore has an explicit success result');
 // [EN] Full workflow: a failed restore between scenarios must not energize the next channel.
 w.D.p[11]=w.D.p[12]=w.D.p[16]=w.D.p[18]=w.D.p[19]=0;w.D.p[15]=w.D.p[17]=0;
 w.winfo=async()=>({fs:1,size:0});w.pmAlert=async msg=>{throw Error('unexpected alert '+msg);};
 w.eval('W={run:false};PEND={};');let windows=0;const hw=[];
 w.setv=async(id,v)=>{hw.push([id,v]);if(id===11&&v===0)throw 'restore blocked';w.D.p[id]=v;if(id===19)w.D.fl=v?(w.D.fl|32):(w.D.fl&~32);};
 w.wopen=async()=>{++windows;w.eval('W.winMs=Date.now()');};w.wlog=async()=>{};
 w.wlatch=async()=>({a:()=>100,lo:Array(31).fill(1),hi:Array(31).fill(1),la:Array(31).fill(1),seq:1,fl:39,or:0});
 d.getElementById('wSw').checked=false;d.getElementById('wL').value='5';['BAT1','BAT2','BOTH'].forEach(sc=>d.getElementById('wc'+sc).checked=sc!=='BOTH');
 const capture=w.wStart();await wait(()=>d.getElementById('wGo')&&w.eval('W.choose'),'capture form');d.getElementById('wB1').value='100';d.getElementById('wGo').click();await capture;
 check(windows===1&&!hw.some(([id,v])=>id===12&&v===1),'a failed inter-scenario restore stops before starting or measuring the next channel');
 check(!w.eval('W.run')&&d.getElementById('wS0').textContent.includes('تأیید نشد')&&!d.getElementById('wS0').textContent.includes('تنظیمات قبلی برگشت'),'completion no longer falsely claims successful hardware restoration');
 check(w.eval('W.K[0].done&&!W.K[1].done'),'already committed measurements survive a restore failure without inventing the next step');
 console.log('ALL '+checks+' RESTORE AUDIT TESTS PASSED');
 }finally{dom.window.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
