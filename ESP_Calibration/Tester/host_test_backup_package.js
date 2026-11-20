/* [EN] Real-panel ZIP package export, independent ZIP reader and split restore contracts. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert'),cp=require('child_process');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom missing; backup package tests did not run.');process.exit(0);}
const timers=[];let checks=0;
const check=(ok,msg)=>{assert.ok(ok,msg);++checks;console.log('  ok   '+msg);};
const unpack=bytes=>{const result={},b=Buffer.from(bytes);let p=0;while(b.readUInt32LE(p)===0x04034b50){const n=b.readUInt16LE(p+26),extra=b.readUInt16LE(p+28),len=b.readUInt32LE(p+18),name=b.subarray(p+30,p+30+n).toString();result[name]=b.subarray(p+30+n+extra,p+30+n+extra+len).toString();p+=30+n+extra+len;}return result;};
(async()=>{
 const dom=new JSDOM(fs.readFileSync(path.join(__dirname,'../panel_preview.html'),'utf8'),{runScripts:'dangerously',pretendToBeVisual:true,url:'http://localhost/',beforeParse(w){w.scrollTo=()=>{};const set=w.setInterval.bind(w);w.setInterval=(f,t)=>{const id=set(f,t);timers.push(id);return id;};}});
 const w=dom.window,d=w.document;
 try{
 await new Promise(r=>setTimeout(r,80));timers.forEach(id=>w.clearInterval(id));
 w.D={on:1,seq:12,fl:7,p:w.eval('Array.from({length:171},(_,i)=>pdflt(i)??0)'),t:Array(31).fill(0)};w.D.p[13]=w.D.p[14]=500;
 w.eval('CALS=[];W={run:false,K:[]};');
 for(let i=0;i<4;i++)w.CALS.push({sc:'BOTH',d:i*5,use:1,r1:80+i*20+.125,r2:90+i*20+.25,b1:i?i*30.125:-20.5,b2:i?i*31.25:-15.125,ts:100+i});
 w.calinvalidate();w.calsmp();w.calrun();w.caldrop(1,120);w.qput(5,123);
 const rows={ready:1,pending:0,error:0,n1:2,n2:0,r1:[[80,0],[100,205]],r2:[]};
 const dayHeader='# epoch,uptime_s,change_bits,note,mains,fl,chg1,chg2,fault_mask,flags2,vlow_mv,vhigh_mv,dead_mask,vin_mv,i1_ma,i2_ma,duty1_permille,duty2_permille\n';
 const requests=[],downloads=[];let bad='',bodyWarning=false,refuse=false,hold=null,bodyHold=false,daysText=null,sourceFlag='';
 w.sl=async()=>{};w.pmName=async name=>name;w.pmConfirm=async()=>!refuse;w.pmAlert=async()=>{};
 w.fetch=async(url,opt={})=>{requests.push([url,opt.method||'GET']);if(hold)await hold;
  if(bodyHold&&url==='/slow')return {ok:true,text:()=>new Promise((resolve,reject)=>opt.signal.addEventListener('abort',()=>reject(Error('aborted body'))))};
  if(url===bad)return {ok:false,status:503,text:async()=>''};
  let v=url==='/lut/read'?{ok:1}:url==='/lut'?{read:rows}:url==='/benchlog?i=1'?{fs:1,size:42,format:'compact',max:102400}:url==='/evt/pct'?{ok:1,p:85}:url==='/evt/days'?(daysText??'00000000\n20261101'):url==='/benchlog'?'# cols:\nBOTH,1,-0.125,یادداشت\n':url.startsWith('/evt/dl?day=')?dayHeader+url+'\n'+(bodyWarning?'# WARNING: read error; export incomplete\n':''):url==='/evt'?'آخرین ردیف‌ها\n':'';
  return {ok:true,status:200,headers:{get:k=>k==='X-Log-Fs'&&url==='/evt/days'?sourceFlag:null},text:async()=>typeof v==='string'?v:JSON.stringify(v)};
 };
 class TestBlob extends w.Blob{constructor(parts,o){super(parts,o);this.parts=parts;}}
 w.Blob=TestBlob;const blobs=new Map();w.URL.createObjectURL=b=>{const u='blob:test-'+blobs.size;blobs.set(u,b);return u;};w.URL.revokeObjectURL=()=>{};
 w.HTMLAnchorElement.prototype.click=function(){downloads.push({name:this.download,bytes:Buffer.concat(blobs.get(this.href).parts.map(p=>Buffer.from(p)))});};
 const find=(files,prefix)=>Object.keys(files).find(k=>k.startsWith(prefix));
 const realSamples=JSON.stringify(w.CALS),originalPending=w.eval('JSON.stringify(PEND)'),view=w.eval('JSON.stringify(calview())');
 check(await w.xexp()===true,'full export succeeds without external libraries or server ZIP support');
 const first=downloads.at(-1),files=unpack(first.bytes),manifest=JSON.parse(files['manifest.json']);
 check(first.name.endsWith('.zip')&&manifest.app==='ChangeOver-package'&&manifest.complete,'single ZIP download has explicit complete status');
 check(Object.keys(files).every(n=>n==='manifest.json'||n.startsWith('restore/')||n.startsWith('archive/')),'restore and PC-only archives have separate top-level folders');
 check(Object.keys(files).filter(n=>n.startsWith('restore/')).length===2,'only settings and sample JSON belong in restore');
 check(!Object.keys(files).some(n=>/readme|guide|راهنما/i.test(n)),'long user guide is not embedded in ZIP or flash');
 const settings=JSON.parse(files[find(files,'restore/settings_')]),samples=JSON.parse(files[find(files,'restore/samples_')]);
 check(settings.v===5&&!('lut' in settings)&&!('benchSamples' in settings)&&!('benchView' in settings),'settings file does not mix in tables or samples');
 check(JSON.stringify(samples.samples)===JSON.stringify(w.xsamples(JSON.parse(realSamples)))&&JSON.stringify(samples.view)===view,'separate sample JSON retains fractional signed source data and exclusions exactly');
 check(settings.pending[5]===123&&settings.schema.length===w.xschema().length,'settings retain unsent edits and parameter identity metadata');
 check(manifest.files.every(f=>files[f.path]!==undefined&&Buffer.byteLength(files[f.path])===f.bytes&&/^[0-9a-f]{8}$/.test(f.crc32)),'manifest enumerates exact UTF-8 byte counts and CRCs for every data file');
 const tag=first.name.slice('changeover-package-'.length,-4);
 check(Object.keys(files).filter(n=>n!=='manifest.json').every(n=>n.includes('_'+tag+'.')),'one shared timestamp identifies all files in the package');
 check(files[Object.keys(files).find(n=>n.startsWith('archive/bench_')&&n.endsWith('.csv'))].includes('-0.125,یادداشت'),'bench CSV bytes preserve signs, decimals and Persian text');
 check(Object.keys(files).filter(n=>/^archive\/history_\d{8}_/.test(n)).length===2,'all listed days including no-clock history get distinct CSV files');
 const boardCsv=Object.keys(files).find(n=>n.startsWith('archive/tables_board_')&&n.endsWith('.csv'));
 check(files[boardCsv].includes('1,100,20.5'),'PC table CSV converts wire deci-mA to labeled mA');
 check(Object.keys(files).some(n=>n.startsWith('archive/tables_generated_')&&n.endsWith('.h'))&&Object.keys(files).some(n=>n.startsWith('archive/tables_cached_')),'generated code and cached table evidence are archived separately from fresh readback');
 check(JSON.parse(files[find(files,'archive/history_share_')]).p===85,'ESP history share is retained for documented manual recovery');
 check(requests.filter(([u,m])=>m!=='GET').every(([u,m])=>u==='/lut/read'&&m==='POST'),'only readback request is posted: no SET, LUT upload, reboot, erase or log writes');
 check(JSON.stringify(w.CALS)===realSamples&&w.eval('JSON.stringify(PEND)')===originalPending&&!w.eval('CAL_BUSY||XBACKUP'),'export preserves source samples and pending edits and releases locks');
 const independent=cp.spawnSync('python3',['-c',`import io,sys,zipfile,json
z=zipfile.ZipFile(io.BytesIO(sys.stdin.buffer.read()))
assert z.testzip() is None
assert all(i.compress_type==zipfile.ZIP_STORED for i in z.infolist())
m=json.loads(z.read('manifest.json'))
for f in m['files']:
 b=z.read(f['path']); assert len(b)==f['bytes']; assert format(z.getinfo(f['path']).CRC,'08x')==f['crc32']
print(len(z.infolist()))`],{input:first.bytes});
 check(independent.status===0,'Python standard ZIP reader independently extracts all entries and verifies CRCs and central directory');
 let writes=0;w.setv=async()=>{++writes;};w.eval('PEND={};CALS=[];');
 await w.ximp({text:async()=>JSON.stringify(settings)});w.pmSampleImport=async()=> 'replace';await w.calimp({text:async()=>JSON.stringify(samples)});
 check(w.eval('PEND[5]')===123&&JSON.stringify(w.CALS)===JSON.stringify(samples.samples)&&w.eval('JSON.stringify(calview())')===view&&writes===0,'extracting and importing the two restore files recovers edits, samples and choices with zero board writes');
 const before=requests.length,count=downloads.length;w.pmName=async()=>null;
 check(await w.xexp()===false&&requests.length===before&&downloads.length===count&&!w.eval('CAL_BUSY||XBACKUP'),'filename cancellation does not fetch board/log data or leave a lock');w.pmName=async n=>n;
 bad='/evt/dl?day=20261101';check(await w.xexp()===true,'explicitly approved partial download remains available');
 let partial=unpack(downloads.at(-1).bytes),pm=JSON.parse(partial['manifest.json']);
 check(!pm.complete&&pm.errors.some(e=>e.includes('20261101'))&&!Object.keys(partial).some(n=>n.startsWith('archive/history_20261101_'))&&d.getElementById('xst').textContent.includes('ناقص'),'failed day is named, omitted, and never presented as a complete package');
 refuse=true;const num=downloads.length;check(await w.xexp()===false&&downloads.length===num,'declining a partial package produces no download');refuse=false;
 check(await w.xexp(true)===false,'a partial safety backup never authorizes calibration writes');
 bad='';bodyWarning=true;await w.xexp();pm=JSON.parse(unpack(downloads.at(-1).bytes)['manifest.json']);
 check(!pm.complete&&pm.errors.some(e=>e.includes('هشدار')),'HTTP 200 containing codec damage/read warnings is marked incomplete');bodyWarning=false;
 w.D.p[5]=null;await w.xexp();pm=JSON.parse(unpack(downloads.at(-1).bytes)['manifest.json']);check(!pm.complete&&pm.unknownSettings.includes(5),'unknown cached parameters cannot silently become a complete settings snapshot');w.D.p[5]=0;
 const old=rows.r1;rows.r1=[[80,'bad']];await w.xexp();pm=JSON.parse(unpack(downloads.at(-1).bytes)['manifest.json']);check(!pm.complete&&pm.errors.some(e=>e.includes('ساختار جدول')),'malformed board table is reported instead of silently dropping points');rows.r1=old;
 w.D.on=0;await w.xexp();pm=JSON.parse(unpack(downloads.at(-1).bytes)['manifest.json']);check(!pm.complete&&pm.errors.some(e=>e.includes('آفلاین')),'offline data is explicitly marked cached/incomplete');w.D.on=1;
 let release;hold=new Promise(r=>release=r);const beforeValue=w.D.p[5],beforeEdit=w.eval('PEND[5]');const op=w.xexp();await new Promise(r=>setTimeout(r,10));
 check(w.eval('CAL_BUSY&&XBACKUP')&&await w.xexp()===false,'only one export runs and capture/import transaction ownership is retained during collection');w.D.p[5]=999;w.qput(5,456);release();hold=null;await op;
 const frozen=unpack(downloads.at(-1).bytes),frozenSettings=JSON.parse(frozen[find(frozen,'restore/settings_')]);
 check(frozenSettings.params[5]===beforeValue&&frozenSettings.pending[5]===beforeEdit,'in-flight telemetry and newer edits cannot change the frozen settings snapshot');w.D.p[5]=beforeValue;
 for(const list of ['../bad','20261101 20261101']){daysText=list;await w.xexp();pm=JSON.parse(unpack(downloads.at(-1).bytes)['manifest.json']);check(!pm.complete&&pm.errors.some(e=>e.includes('فهرست روزها')),'malformed/duplicate day lists cannot become archive paths or false empty history');}daysText=null;
 sourceFlag='3';await w.xexp();pm=JSON.parse(unpack(downloads.at(-1).bytes)['manifest.json']);check(!pm.complete&&pm.errors.some(e=>e.includes('حافظهٔ تاریخچه')),'HTTP 200 with unmounted history filesystem is not accepted as an empty successful history');sourceFlag='';
 w.eval('W.run=true');const n=requests.length;check(await w.xexp()===false&&requests.length===n,'active bench capture cannot start a package read');w.eval('W.run=false');
 w.eval('CAL_BUSY=true');check(await w.xexp()===false&&w.eval('CAL_BUSY'),'unrelated transaction cannot be bypassed or unlocked by normal export');
 await w.xexp(true);check(w.eval('CAL_BUSY')&&!w.eval('XBACKUP'),'nested safety export preserves the caller-owned calibration lock');w.eval('CAL_BUSY=false');
 bodyHold=true;const timeout=w.setTimeout;w.setTimeout=(fn,ms)=>timeout(fn,ms===5000?15:ms);let aborted=false;try{await w.xread('/slow');}catch(e){aborted=true;}w.setTimeout=timeout;check(aborted,'five-second abort covers a stalled response body, not only response headers');
 bodyHold=false;const invalidRequests=requests.length;w.CALS.push({sc:'BOTH',r1:null});check(await w.xexp()===false&&requests.length===invalidRequests&&!w.eval('CAL_BUSY||XBACKUP'),'invalid local samples abort before network reads and release the export lock');
 w.eval('CALS=[];CALR=[];CALCUT=[[],[]]');await w.xexp();pm=JSON.parse(unpack(downloads.at(-1).bytes)['manifest.json']);check(pm.complete&&pm.absent.some(e=>e.includes('نمونه')),'true absence of local samples is recorded separately from a collection failure');
 console.log('ALL '+checks+' BACKUP PACKAGE TESTS PASSED');
 }finally{dom.window.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
