/* [EN] Real-panel regressions for editable exports, shared file names and actionable diagnostics. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom missing; calibration revision tests did not run.');process.exit(0);}
const timers=[];let checks=0;
function check(ok,msg){assert.ok(ok,msg);++checks;console.log('  ok   '+msg);}
const pause=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
 const dom=new JSDOM(fs.readFileSync(process.env.PANEL_CAL_BASELINE||path.join(__dirname,'../panel_preview.html'),'utf8'),{runScripts:'dangerously',pretendToBeVisual:true,url:'http://localhost/',beforeParse(w){w.scrollTo=()=>{};const set=w.setInterval.bind(w);w.setInterval=(f,t)=>{const id=set(f,t);timers.push(id);return id;};}});
 const w=dom.window,d=w.document;
 try{
 await pause(80);timers.forEach(id=>w.clearInterval(id));
 const wait=async(pred,label)=>{const end=Date.now()+2500;while(!pred()){if(Date.now()>end)throw Error('timeout: '+label);await pause(5);}};
 w.D={on:1,seq:12,fl:7,p:Array(171).fill(0),t:Array(30).fill(0)};w.D.p[2]=w.D.p[3]=1000;
 const downloads=[],blobs=new Map(),requests=[];
 class TestBlob extends w.Blob{constructor(parts,options){super(parts,options);this.parts=parts;}}
 w.Blob=TestBlob;w.URL.createObjectURL=blob=>{const u='blob:test-'+blobs.size;blobs.set(u,blob);return u;};w.URL.revokeObjectURL=()=>{};
 w.HTMLAnchorElement.prototype.click=function(){downloads.push({name:this.download,href:this.href,blob:blobs.get(this.href)});};
 w.fetch=async url=>{requests.push(String(url));return {ok:true,blob:async()=>new TestBlob(['CSV '+url]),text:async()=>'',json:async()=>w.D};};
 const seed=()=>{w.eval('CALS=[];W={run:false};');for(let i=0;i<6;i++)w.CALS.push({sc:'BOTH',d:i*5,use:1,r1:80+i*20+(i===1?.125:0),r2:90+i*20,b1:i?i*26.25-20:-20.5,b2:i?i*27.3-15:-15.125,ts:1000+i});Object.keys({BAT1:1,BAT2:1,BOTH:1}).forEach(sc=>d.getElementById('wc'+sc).checked=true);w.calinvalidate();w.calsmp();w.calrun();};
 const names=async(value,ok=true)=>{await wait(()=>d.getElementById('pmod').classList.contains('on')&&d.getElementById('pmfilename'),'filename dialog');if(value!==undefined)d.getElementById('pmfilename').value=value;d.getElementById(ok?'pmok':'pmno').click();};
 const contents=download=>download.blob.parts.join('');
 seed();w.calcode();w.caldrop(1,140);w.caluse(1,false);
 check(!w.calbuild(1).X.includes(140)&&!w.calbuild(1).X.includes(100)&&d.querySelector('#calpoints table'),'sample deselection automatically updates generated tables while retaining point deletions');
 check(w.eval('CALR.length')>0&&!d.getElementById('calcd').value,'recalculation produces a current result and invalidates stale code');
 const original=JSON.stringify(w.CALS),expected=[JSON.stringify(w.calbuild(1)),JSON.stringify(w.calbuild(2))];
 const exportPromise=w.calexp();await wait(()=>d.getElementById('pmfilename'),'sample export name');
 check(!downloads.length,'sample export does not start before filename confirmation');
 await names('اندازه‌گیری اصلاح‌شده');await exportPromise;
 const archive=JSON.parse(contents(downloads.at(-1)));
 check(downloads.at(-1).name==='اندازه‌گیری اصلاح‌شده.json'&&archive.v===2,'editable Persian name is used and required extension is retained');
 check(archive.view.cuts[0][0]===140&&archive.samples[1].use===0,'saved sample file includes per-channel point deletions and sample selection');
 check(archive.samples[1].r1===100.125&&archive.samples[0].b1===-20.5&&archive.samples[0].b2===-15.125,'edited export preserves original fractional raw means and signed readings exactly');
 w.eval('CALS=[];');w.calinvalidate();w.pmConfirm=async()=>false;w.pmSampleImport=async()=> 'replace';
 await w.calimp({text:async()=>JSON.stringify(archive)});
 check(JSON.stringify(w.calbuild(1))===expected[0]&&JSON.stringify(w.calbuild(2))===expected[1],'importing the edited file regenerates exactly the same two tables');
 check(require('util').isDeepStrictEqual(JSON.parse(JSON.stringify(w.CALS)),JSON.parse(original))&&w.eval('CALR.length')>0,'edited-file round trip preserves all source data and recalculates automatically');
 const codeOp=w.calcdl();await names('جدول-جدید.h');await codeOp;
 check(contents(downloads.at(-1)).includes('80u, 120u, 160u, 180u')&&!contents(downloads.at(-1)).includes('80u, 100u, 120u, 140u'),'downloaded generated code reflects current deletions, not the old table');
 const preserved=JSON.stringify(w.CALS),mask=w.eval('JSON.stringify(CALCUT)');
 for(const view of [{cuts:[[80],[]]},{cuts:[[999],[]]},{cuts:[['140'],[]]},{cuts:[[],[]],scenarios:['__proto__']},null]){await w.calimp({text:async()=>JSON.stringify({...archive,view})});check(JSON.stringify(w.CALS)===preserved&&w.eval('JSON.stringify(CALCUT)')===mask,'invalid edited-file metadata is rejected without changing current calibration');}
 check(w.calnorm({...archive.samples[0],sc:'__proto__'})===null,'unknown inherited scenario names cannot reach table builders');
 const legacy={app:'ChangeOver-bench-samples',v:1,samples:archive.samples};await w.calimp({text:async()=>JSON.stringify(legacy)});
 check(w.eval('CALCUT[0].length')===0&&w.CALS[1].use===0,'legacy sample files remain readable with explicit empty point exclusions');
 seed();w.caldrop(1,140);w.eval('LBT={ready:true,T:[{X:[1,2],Y:[1,2]},{X:[3,4],Y:[3,4]}]}');
 const read=w.xread,delay=w.sl;w.sl=async()=>{};w.xread=async u=>u==='/lut/read'?{ok:1}:u==='/lut'?{read:{ready:1,n1:0,n2:0,r1:[],r2:[]}}:u==='/benchlog?i=1'?{fs:1}:u==='/benchlog'?'# cols:\n':u==='/evt/pct'?{p:100}:'';
 const backupOp=w.xexp();await names('بسته-جدید');await backupOp;w.xread=read;w.sl=delay;
 const packed=Buffer.from(downloads.at(-1).blob.parts[0]),entries={};let at=0;
 while(packed.readUInt32LE(at)===0x04034b50){const len=packed.readUInt32LE(at+18),n=packed.readUInt16LE(at+26),e=packed.readUInt16LE(at+28),start=at+30+n+e;entries[packed.subarray(at+30,at+30+n).toString()]=packed.subarray(start,start+len).toString();at=start+len;}
 const backup=JSON.parse(entries[Object.keys(entries).find(n=>n.startsWith('restore/settings_'))]),bench=JSON.parse(entries[Object.keys(entries).find(n=>n.startsWith('restore/samples_'))]);
 check(bench.view.cuts[0][0]===140&&downloads.at(-1).name==='بسته-جدید.zip','package retains current point choices in its independent sample file and keeps the ZIP extension');
 w.calinvalidate();w.pmConfirm=async()=>true;await w.ximp({text:async()=>JSON.stringify(backup)});await w.calimp({text:async()=>JSON.stringify(bench)});
 check(w.eval('CALCUT[0][0]')===140&&w.eval('CALR.length')>0&&!w.calbuild(1).X.includes(140)&&d.querySelector('#calpoints table'),'separate package restore files regenerate the edited calibration tables without writing to the board');
 const beforeCancel=downloads.length,requestCount=requests.filter(u=>u==='/benchlog').length;
 const canceled=w.wdl();await names(undefined,false);check(await canceled===false&&downloads.length===beforeCancel&&requests.filter(u=>u==='/benchlog').length===requestCount,'canceling a download name performs no fetch, blob allocation or download');
 const remote=w.wdl();await names('بنچ-آزمایش.csv');await remote;
 check(downloads.at(-1).name==='بنچ-آزمایش.csv'&&downloads.at(-1).href.startsWith('blob:')&&requests.at(-1)==='/benchlog','remote bench download uses the chosen filename independently of server Content-Disposition');
 const dayOp=w.evtdl();await names('روز-من');await dayOp;check(downloads.at(-1).name==='روز-من.csv','daily history download requires and uses filename confirmation');
 let dayCalls=0;w.evtDays=async()=>{++dayCalls;return ['20261101','20261102'];};w.evtHttp=async url=>({ok:true,text:async()=>url+'\n'});
 const allCancel=w.evtdlall();await names(undefined,false);await allCancel;check(dayCalls===0,'canceling combined history does not read any day files');
 const allOp=w.evtdlall();await names('همه-روزها');await allOp;check(downloads.at(-1).name==='همه-روزها.csv'&&contents(downloads.at(-1)).includes('day=20261102'),'combined history confirms its name before collecting complete file data');
 const invalid=w.wdl();await wait(()=>d.getElementById('pmfilename'),'invalid name dialog');const beforeInvalid=downloads.length;
 for(const n of ['', '../bad.csv','bad\\file.csv','bad\u0001.csv','..']){d.getElementById('pmfilename').value=n;d.getElementById('pmok').click();check(d.getElementById('pmod').classList.contains('on')&&downloads.length===beforeInvalid,'empty/path/control-only filename is blocked without triggering a download');}
 d.getElementById('pmfilename').value='مجاز.csv';d.dispatchEvent(new w.KeyboardEvent('keydown',{key:'Enter',bubbles:true}));await invalid;
 check(downloads.at(-1).name==='مجاز.csv'&&!d.getElementById('pmfilename'),'Enter accepts a valid filename and cleans up the input');
 const queued1=w.wdl(),queued2=w.evtdl();await names(undefined,false);await queued1;await names('دوم');await queued2;
 check(downloads.at(-1).name==='دوم.csv','concurrent save requests queue independent filename confirmations');
 const raw=w.fetch;w.fetch=async()=>({ok:false});let errorText='';const alert=w.pmAlert;w.pmAlert=async msg=>errorText=msg;
 const failed=w.wdl();await names('خراب');check(await failed===false&&errorText.includes('فایل آماده نشد'),'file read failure is reported and does not create an empty download');w.fetch=raw;w.pmAlert=alert;
 const source=fs.readFileSync(path.join(__dirname,'../plink_panel.h'),'utf8');check((source.match(/\.download=/g)||[]).length===1&&!/download="/.test(source),'all panel download paths use the single confirmed-name dispatcher');
 seed();w.CALS[4].b1=700;w.calsmp();w.calrun();
 check(d.querySelector('[data-csample="4"]').dataset.calissue==='warn'&&d.querySelector('[data-csample="5"]').dataset.calissue==='warn','both sides of a nonmonotonic pair blink as suspects rather than claiming a uniquely wrong reading');
 check(d.querySelector('[data-cpoint="1:160"]').dataset.calissue==='warn'&&!d.querySelector('[data-cpoint="2:170"]').dataset.calissue,'generated-point diagnostics are isolated to the affected battery');
 check(d.querySelector('[data-csample="4"] [data-calhint]').textContent.includes('بررسی'),'suspect rows include visible readable reasons, not only animation or color');
 w.caluse(4,false);check(!d.querySelector('[data-csample="4"]').dataset.calissue&&!d.querySelector('[data-cpoint="1:160"]'),'excluding a suspect measurement recalculates and clears its obsolete warning');
 seed();w.CALS[2].b2=null;w.calsmp();w.calrun();check(d.querySelector('[data-csample="2"]').dataset.calissue==='error','an incomplete measurement has a distinct definite-error marker');
 seed();w.CALS[3].d=w.CALS[2].d;w.calsmp();w.calrun();check(d.querySelector('[data-csample="2"]').dataset.calissue==='warn'&&d.querySelector('[data-csample="3"]').dataset.calissue==='warn','duplicate-duty warnings identify every involved sample');
 const style=[...d.querySelectorAll('style')].map(e=>e.textContent).join('');check(style.includes('calpulse 2.4s')&&style.includes('prefers-reduced-motion:reduce'),'attention uses slow background-only pulses and a static reduced-motion alternative');
 w.eval('W={run:false,K:[{sc:"BOTH",i:0,d:0,issue:"fault"}]}');w.wbuild(['BOTH'],[0]);w.eval('W.K[0].issue="fault"');w.calmark();check(d.getElementById('wr0').dataset.calissue==='error','a failed bench step blinks even when it has no committed sample');
 const before=w.CALS[0].use;w.eval('W.run=true');w.caluse(0,false);check(w.CALS[0].use===before,'sample mutation is refused while capture owns the hardware');w.eval('W.run=false');

 seed();w.eval('PEND={};');
 let commands=0;const send=w.setv;w.setv=async()=>{++commands;};
 const applyOp=w.calmodalApply();await names(undefined,false);await applyOp;
 check(commands===0&&!w.eval('CAL_BUSY'),'canceling the automatic backup filename also cancels parameter application and releases the transaction lock');w.setv=send;
 const summary=w.calsummary([['gain',2,1000,1100,'bad fit',0]],null,'test');
 check(summary.includes('data-calissue="error"'),'a blocked calculated parameter is an identifiable blinking result row');
 const base={ready:true,stamp:1,T:[{X:[80,100],Y:[0,25]},{X:[],Y:[]}]},proposal={target:1,body:'test',T:[{X:[80,100],Y:[0,30]},{X:[],Y:[]}]};
 w.lrender(proposal,base,null);check(!d.querySelector('#lutcmp tr[data-calissue]'),'an uncommitted proposal is not mislabeled as a failed readback');
 w.lrender(proposal,base,{...base,stamp:2});check(!!d.querySelector('#lutcmp tr[data-calissue="error"]'),'a real post-commit mismatch highlights its own row');
 w.lrender(proposal,base,{ready:true,stamp:3,T:proposal.T});check(!d.querySelector('#lutcmp tr[data-calissue]'),'matching readback removes obsolete mismatch attention');
 const noData=w.wdl();await wait(()=>d.getElementById('pmfilename'),'escape name');d.dispatchEvent(new w.KeyboardEvent('keydown',{key:'Escape',bubbles:true}));
 check(await noData===false,'Escape cancels the filename dialog without downloading');
 console.log('ALL '+checks+' CALIBRATION REVISION TESTS PASSED');
 }finally{dom.window.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
