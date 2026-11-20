/* [EN] Real Chromium download/safety integration using responses emitted by the real C++ handlers. */
'use strict';
const fs=require('fs'),path=require('path'),http=require('http'),assert=require('assert'),cp=require('child_process');
let chromium,engine;
try{({chromium}=require('playwright-core'));if(!process.env.CHROMIUM_PATH)engine=require('@sparticuz/chromium');}
catch(e){console.log('SKIP: install playwright-core and Chromium; backup browser integration did not run.');process.exit(0);}
let checks=0;
const check=(ok,message)=>{assert.ok(ok,message);++checks;console.log('  ok   '+message);};
const build=path.join(__dirname,'build');
const fixtures=JSON.parse(fs.readFileSync(path.join(build,'backup_responses.json'),'utf8'));
const source=fs.readFileSync(path.join(__dirname,'../plink_panel.h'),'utf8');
const html=source.split('R"HTML(')[1].split(')HTML"')[0];
const fontSource=fs.readFileSync(path.join(__dirname,'../plink_font.h'),'utf8');
const fontStart=fontSource.indexOf('=',fontSource.indexOf('static const char ESP_PANEL_FONT_CSS'))+1;
const fontLiteral=fontSource.slice(fontStart,fontSource.indexOf(';\n',fontStart));
const font=[...fontLiteral.matchAll(/"(?:\\.|[^"\\])*"/g)].map(m=>JSON.parse(m[0])).join('');
let mode='healthy',hold=false,release=null;
const requests=[],pageErrors=[];
const server=http.createServer(async(req,res)=>{
 requests.push({url:req.url,method:req.method});
 if(req.url==='/'){res.writeHead(200,{'Content-Type':'text/html; charset=utf-8'});return res.end(html);}
 if(req.url.startsWith('/f.css')){res.writeHead(200,{'Content-Type':'text/css'});return res.end(font);}
 if(hold&&req.url==='/lut/read')await new Promise(resolve=>release=resolve);
 let key=req.url;
 if(key==='/s?id=19&v=0')key='manualExit';
 if(mode==='benchDamaged'&&key==='/benchlog')key='benchDamaged';
 if(mode==='dayDamaged'&&key==='/evt/dl?day=20261101')key='dayDamaged';
 if(mode==='historyUnmounted'&&key==='/evt/days')key='historyUnmounted';
 if(mode==='lutBusy'&&key==='/lut/read')key='lutBusy';
 const fixture=fixtures[key];
 if(!fixture){res.writeHead(404);return res.end('unknown test route');}
 res.writeHead(fixture.status,{'Content-Type':'text/plain; charset=utf-8',...fixture.headers});res.end(fixture.body);
});
const readZip=file=>{
 const result=cp.spawnSync('python3',['-c',`import sys,zipfile,json,zlib
z=zipfile.ZipFile(sys.argv[1]); assert z.testzip() is None
m=json.loads(z.read('manifest.json'))
for f in m['files']:
 b=z.read(f['path']); assert len(b)==f['bytes']; assert format(zlib.crc32(b),'08x')==f['crc32']
def member(prefix,suffix):
 names=[n for n in z.namelist() if n.startswith(prefix) and n.endswith(suffix)]
 return z.read(names[0]).decode() if names else None
print(json.dumps({'manifest':m,'names':z.namelist(),'settings':member('restore/settings_','.json'),'samples':member('restore/samples_','.json'),'bench':member('archive/bench_','.csv'),'day':member('archive/history_20261101_','.csv'),'board':member('archive/tables_board_','.csv')}))`,file],{encoding:'utf8'});
 assert.strictEqual(result.status,0,result.stderr);return JSON.parse(result.stdout);
};
(async()=>{
 let browser;
 const watchdog=setTimeout(()=>{console.error('FAIL: backup browser watchdog expired');process.exit(1);},90000);
 try{
 await new Promise(resolve=>server.listen(0,'0.0.0.0',resolve));
 const launch={headless:true};
 if(engine){const lib=path.join(await engine.inflate(path.join(path.dirname(require.resolve('@sparticuz/chromium')),'../bin/al2023.tar.br')),'lib');launch.executablePath=await engine.default.executablePath();launch.args=engine.default.args;launch.env={...process.env,LD_LIBRARY_PATH:lib+':'+(process.env.LD_LIBRARY_PATH||'')};}
 else{launch.executablePath=process.env.CHROMIUM_PATH;launch.args=['--no-sandbox'];}
 browser=await chromium.launch(launch);
 const page=await browser.newPage({acceptDownloads:true});
 page.on('pageerror',e=>pageErrors.push(String(e)));
 await page.addInitScript(()=>{window.setInterval=()=>0;const old=window.setTimeout;window.setTimeout=(f,ms,...args)=>f?.name==='poll'?0:old(f,ms,...args);});
 await page.goto('http://127.0.0.1:'+server.address().port);
 await page.waitForFunction(()=>D&&D.p&&D.on===1);
 await page.evaluate(()=>{
  CALS=[];W={run:false,K:[]};
  for(let i=0;i<4;i++)CALS.push({sc:'BOTH',d:i*5,use:1,r1:80+i*20+.125,r2:90+i*20+.25,b1:i?i*30.125:-20.5,b2:i?i*31.25:-15.125,ts:100+i});
  calinvalidate();calsmp();calrun();caldrop(1,120);qput(5,123);
 });
 await page.locator('nav [data-t="2"]').click();await page.locator('#sbt [data-s="4"]').click();
 const state=()=>page.evaluate(()=>JSON.stringify({CALS,CALCUT,PEND}));
 const original=await state();let downloaded=0;page.on('download',()=>++downloaded);
 const start=async()=>{await page.locator('#s4 button[onclick="xexp()"]').click();await page.locator('#pmfilename').waitFor();};
 const name=async()=>{await page.locator('#pmfilename').fill('پشتیبان آزمون.zip');await page.locator('#pmok').click();};
 const save=async(name,pending)=>{const d=await pending;const file=path.join(build,name);await d.saveAs(file);await page.waitForFunction(()=>!XBACKUP);return readZip(file);};
 const marker=requests.length;await start();await page.locator('#pmno').click();await page.waitForFunction(()=>!XBACKUP);
 check(downloaded===0&&requests.slice(marker).every(r=>r.url==='/t'),'canceling the actual filename dialog performs no archive requests or downloads');
 check(await state()===original&&!await page.evaluate(()=>CAL_BUSY),'filename cancellation preserves samples/pending edits and frees hardware ownership');
 hold=true;await start();const zipEvent=page.waitForEvent('download');await name();
 await page.waitForFunction(()=>XBACKUP&&CAL_BUSY);while(!release)await new Promise(r=>setTimeout(r,5));
 check(await page.locator('#lbtnR').isDisabled(),'table reset is disabled while a real backup request is held');
 const heldStart=requests.length;
 await page.evaluate(async()=>{
  await wStart();await sendall();await calapply();await lrst();await wclear();await calclr();
  await ximp({text:async()=>JSON.stringify({app:'ChangeOver-settings',v:5,params:{5:999}})});
  await calimp({text:async()=>JSON.stringify({app:'ChangeOver-bench-samples',v:2,view:{cuts:[[],[]]},samples:CALS})});
  calpick(false);caldelete(0);await xexp();
 });
 check(await state()===original,'capture, send, reset, clear, imports, sample edits and duplicate export cannot mutate the held backup session');
 check(requests.slice(heldStart).length===0,'blocked competing actions emit no HTTP command');
 await page.evaluate(()=>poll());
 check(requests.slice(heldStart).some(r=>r.url==='/t'),'telemetry polling still works while the backup lock is held');
 const manualRequest=page.waitForResponse(r=>r.url().endsWith('/s?id=19&v=0'));
 await page.evaluate(()=>document.getElementById('mx').click());await manualRequest;
 check(requests.slice(heldStart).filter(r=>r.method==='POST').length===1&&requests.at(-1).url==='/s?id=19&v=0','intentional manual-exit control remains available during backup instead of being frozen by its lock');
 hold=false;release();release=null;
 let zip=await save('backup-browser-healthy.zip',zipEvent);
 check(zip.manifest.complete&&zip.manifest.errors.length===0,'real Chromium downloads a complete ZIP from actual production-handler response bodies');
 check(zip.bench===fixtures['/benchlog'].body,'ZIP bench CSV is byte-identical to the real compact-codec HTTP download');
 check(zip.day===fixtures['/evt/dl?day=20261101'].body,'ZIP day CSV is byte-identical to the actual reconstructed daily history');
 check(zip.board.includes('1,100,20.5'),'actual UART readback value 205 deci-mA is displayed as 20.5 mA in the PC archive');
 check(zip.names.filter(n=>n.startsWith('restore/')).length===2&&!('lut' in JSON.parse(zip.settings)),'restorable files remain separate from archived tables and logs');
 check(await state()===original&&!await page.evaluate(()=>CAL_BUSY||XBACKUP),'successful real-browser download retains local edits and releases both locks');
 check(requests.slice(marker).filter(r=>r.method==='POST'&&r.url!=='/s?id=19&v=0').every(r=>r.url==='/lut/read'),'apart from the explicitly tested manual exit, backup network traffic contains no parameter/table write, reset, clear or format request');
 for(const fault of ['benchDamaged','dayDamaged','historyUnmounted','lutBusy']){
  mode=fault;await start();const op=page.waitForEvent('download');await name();await page.waitForFunction(()=>document.querySelector('#pmod').classList.contains('on')&&document.querySelector('#pmt').textContent.includes('بسته ناقص'));
  check(await page.evaluate(()=>CAL_BUSY&&XBACKUP),'partial confirmation retains the transaction lock for '+fault);
  await page.locator('#pmok').click();zip=await save('backup-browser-'+fault+'.zip',op);
  check(!zip.manifest.complete&&zip.manifest.errors.length>0,'actual '+fault+' handler response cannot be reported as a complete package');
  if(fault==='historyUnmounted')check(zip.names.some(n=>n.startsWith('archive/bench_'))&&!zip.names.some(n=>n.startsWith('archive/history_20261102_')),'unavailable day list is not fabricated while independent bench data is retained');
  else check(zip.names.some(n=>n.startsWith('archive/history_20261102_')),'later healthy days remain included after '+fault);
 }
 mode='benchDamaged';await start();await name();await page.waitForFunction(()=>document.querySelector('#pmod').classList.contains('on')&&document.querySelector('#pmt').textContent.includes('بسته ناقص'));
 const beforeReject=downloaded;await page.locator('#pmno').click();await page.waitForFunction(()=>document.querySelector('#pmt').textContent.includes('فایل آماده نشد')&&document.querySelector('#pmod').classList.contains('on'));await page.locator('#pmok').click();await page.waitForFunction(()=>!XBACKUP);
 check(downloaded===beforeReject&&!await page.evaluate(()=>CAL_BUSY),'declining the real partial-package dialog produces no download and releases the lock after its error acknowledgment');
 // [EN] Exercise actual pre-apply control flow, not a mocked xexp result.
 await page.evaluate(()=>{window.__safetyWrites=[];window.__realSetv=setv;setv=async(id,value)=>{window.__safetyWrites.push({id,value,backup:XBACKUP,lock:CAL_BUSY});D.p[id]=value;};void calmodalApply();});
 await page.locator('#pmfilename').waitFor();const safetyDownload=page.waitForEvent('download');await name();await page.waitForFunction(()=>document.querySelector('#pmt').textContent.includes('بسته ناقص')&&document.querySelector('#pmod').classList.contains('on'));await page.locator('#pmok').click();await save('backup-browser-safety-partial.zip',safetyDownload);await page.waitForFunction(()=>!CAL_BUSY);
 check(await page.evaluate(()=>window.__safetyWrites.length===0),'retaining a partial safety archive never applies any calibration parameter');
 mode='healthy';
 await page.evaluate(()=>{calrun();void calmodalApply();});await page.locator('#pmfilename').waitFor();
 check(await page.evaluate(()=>CAL_BUSY&&XBACKUP&&window.__safetyWrites.length===0),'pre-apply calibration retains ownership and writes nothing before backup filename confirmation');
 const applyDownload=page.waitForEvent('download');await name();zip=await save('backup-browser-safety-complete.zip',applyDownload);await page.waitForFunction(()=>!CAL_BUSY);
 check(zip.manifest.complete&&await page.evaluate(()=>window.__safetyWrites.length>0&&window.__safetyWrites.every(r=>r.lock&&!r.backup)),'actual pre-apply flow starts selected calibration writes only after a complete archive and retains the caller-owned lock');
 await page.evaluate(()=>{setv=window.__realSetv;});
 // [EN] Larger-than-normal ZIP assembly is browser-only; independently read all CRCs.
 const largeEvent=page.waitForEvent('download');await page.evaluate(()=>{const files=Array.from({length:32},(_,i)=>({name:'archive/stress_'+i+'.csv',text:'0123456789,داده\n'.repeat(5000)}));void filesave('stress.zip',()=>xzip(files),'application/zip');});await page.locator('#pmfilename').waitFor();await page.locator('#pmok').click();const large=await largeEvent;const largeFile=path.join(build,'backup-browser-stress.zip');await large.saveAs(largeFile);
 const stress=cp.spawnSync('python3',['-c',"import zipfile,sys;z=zipfile.ZipFile(sys.argv[1]);assert len(z.infolist())==32 and sum(i.file_size for i in z.infolist())>2000000 and z.testzip() is None",largeFile],{encoding:'utf8'});
 check(stress.status===0,'browser assembles a 32-member, over-2MB UTF-8 ZIP whose complete payloads/CRCs pass an independent reader');
 check(pageErrors.length===0,'all success, fault and stress workflows finish without an uncaught browser error');
 console.log('ALL '+checks+' BACKUP BROWSER SAFETY CHECKS PASSED');
 }finally{clearTimeout(watchdog);if(release)release();if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(e=>{console.error(e);process.exitCode=1;});
