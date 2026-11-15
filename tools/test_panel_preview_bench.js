/* [EN] Integration: real HTTP simulator + real panel + preview controls in jsdom. */
'use strict';
const assert=require('assert'),{spawn}=require('child_process'),path=require('path');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom missing; simulator bench integration did not run.');process.exit(0);}
const pause=ms=>new Promise(r=>setTimeout(r,ms));let count=0;
function check(value,msg){assert.ok(value,msg);++count;console.log('  ok   '+msg);}
(async()=>{
 const child=spawn(process.execPath,[path.join(__dirname,'panel_preview_server.js')],{env:{...process.env,PORT:'0'},stdio:['ignore','pipe','pipe']});let dom;
 try{
 const base=await new Promise((resolve,reject)=>{let logs='';const timer=setTimeout(()=>reject(Error('server startup timeout')),10000);child.stdout.on('data',buf=>{logs+=buf;const m=logs.match(/http:\/\/0\.0\.0\.0:(\d+)/);if(m){clearTimeout(timer);resolve('http://127.0.0.1:'+m[1]);}});child.once('error',reject);child.once('exit',code=>reject(Error('server exited '+code)));});
 dom=await JSDOM.fromURL(base,{runScripts:'dangerously',resources:'usable',pretendToBeVisual:true,beforeParse(w){w.scrollTo=()=>{};w.fetch=(u,o)=>fetch(new URL(u,base),o);}});
 const w=dom.window,d=w.document;
 const wait=async(pred,msg)=>{const end=Date.now()+25000;while(!pred()){if(Date.now()>end)throw Error('timeout '+msg+' / '+d.getElementById('wS0')?.textContent+' / '+d.getElementById('demoExplain')?.textContent+' / '+w.eval('JSON.stringify({W:W,on:D&&D.on,busy:CAL_BUSY})'));await pause(40);}};
 const req=async(url,method='GET',body)=>fetch(base+url,{method,body});
 await wait(()=>d.querySelector('#calpoints table'),'auto demo');w.pmConfirm=async()=>true;
 w.pmAlert=async msg=>{throw Error('unexpected panel alert: '+msg);};
 check(d.getElementById('p1').classList.contains('a'),'simulator opens on the calibration tab');
 check(w.CALS.length===8&&w.calbuild(1).X.length===7,'preview-only initial fixture demonstrates duplicate merging');
 d.getElementById('demoBad').click();await wait(()=>w.CALS[4]?.b1===700,'suspect demo');
 check(!!w.calbuild(1).bad&&!w.calbuild(2).bad,'suspect fixture shows a real table failure without falsely condemning both batteries');
 d.querySelector('#calpoints [onclick="caldrop(1,160)"]').click();
 check(!w.calbuild(1).bad&&w.CALS[4].b1===700,'deleting a suspect output point repairs the table but preserves the original measurement');
 d.getElementById('demoDense').click();await wait(()=>w.CALS.length===28,'dense demo');
 [100,120,140,160].forEach(x=>w.caldrop(1,x));
 check(w.calbuild(1).X.length===24&&w.calbuild(2).X.length===28,'four manual deletions reduce a dense table to the board capacity independently');
 d.getElementById('demoCapture').click();await wait(()=>w.eval('W.current===0&&!!W.choose')&&d.getElementById('wGo'),'capture form');
 check((w.D.fl&32)!==0&&w.D.t[17]===12400&&w.D.t[18]===12600,'real manual-mode requests produce stable safe simulated bench telemetry');
 const json=await (await req('/m')).json();await pause(250);const later=await (await req('/m')).json();
 check(later.n>json.n&&later.seq>json.seq&&later.fl&32,'statistics window accumulates actual ticker frames, sequence and manual flags');
 d.getElementById('demoMeter').click();check(d.getElementById('wB1').value==='-20','preview meter helper supplies a valid signed zero-duty reading');
 d.getElementById('wGo').click();await wait(()=>w.eval('W.current===1&&!!W.choose')&&d.getElementById('wGo'),'next step');
 d.querySelector('[data-wretry="0"]').click();await wait(()=>w.eval('W.current===0&&!!W.choose')&&d.getElementById('wGo'),'repeat earlier step');
 await pause(180);d.getElementById('demoMeter').click();d.getElementById('wGo').click();await wait(()=>w.eval('W.current===1&&!!W.choose')&&d.getElementById('wGo'),'resume');
 check(w.eval('W.K[0].attempts===2&&W.next===1'),'real HTTP capture can repeat an earlier row and resume the interrupted one');
 for(const next of [2,3]){await pause(180);d.getElementById('demoMeter').click();d.getElementById('wGo').click();await wait(()=>next===3?d.getElementById('wReviewEnd'):w.eval('W.current===2&&!!W.choose')&&d.getElementById('wGo'),'advance '+next);}
 check(w.eval('W.run')&&w.D.p[19]===0,'last capture remains in review after restoring original manual mode');
 d.getElementById('wReviewEnd').click();await wait(()=>!w.eval('W.run'),'finish');
 // [EN] Reopen an actually finished session through the same public row control.
 const oldSamples=w.eval('W.K.map(k=>k.savedSample)'),savedRows=w.eval('W.K');
 d.querySelector('[data-wretry="0"]').click();await wait(()=>w.eval('W.run&&W.current===0&&!!W.choose')&&d.getElementById('wGo'),'post-finish recapture');
 await pause(180);d.getElementById('demoMeter').click();d.getElementById('wGo').click();await wait(()=>d.getElementById('wReviewEnd'),'post-finish review');
 check(w.eval('W.K')===savedRows&&w.eval('W.K[0].savedSample')!==oldSamples[0]&&w.eval('W.K[1].savedSample')===oldSamples[1],'finished-session row capture works through real HTTP without rebuilding other rows');
 d.getElementById('wReviewEnd').click();await wait(()=>!w.eval('W.run'),'finish again');
 const csv=await (await req('/benchlog')).text(),info=await (await req('/benchlog?i=1')).json();
 check(csv.includes('# repeat scenario=BOTH step=1')&&csv.split('\n').filter(x=>x.startsWith('BOTH,')).length===5,'downloaded simulator CSV contains all five actual submitted attempts and the repeat annotation');
 check(info.size===Buffer.byteLength(csv),'simulator file size counts actual CSV bytes, not fictional row sizes');
 if(process.env.BENCH_CODEC_FIXTURE)require('fs').writeFileSync(process.env.BENCH_CODEC_FIXTURE,csv);
 const lines=csv.trimEnd().split('\n'),data=lines.filter(x=>x.startsWith('BOTH,')),settings=lines.filter(x=>x.startsWith('# settings:'));
 const config=require('fs').readFileSync(path.join(__dirname,'../ESP_Calibration/plink_config.h'),'utf8');
 const legend=config.split('#define ESP_BENCHLOG_HEADER')[1].split('/* ==================== Wi-Fi')[0];
 const legendBytes=Buffer.byteLength([...legend.matchAll(/"(?:\\.|[^"\\])*"/g)].map(m=>JSON.parse(m[0])).join(''));
 console.log('BENCH_STORAGE_MEASURE '+JSON.stringify({fixture:'3 steps BOTH; 2 repeat attempts',firmwareLegendBytes:legendBytes,measurementColumns:data[0].split(',').length,dataRows:data.length,dataRowBytes:data.map(x=>Buffer.byteLength(x+'\n')),settingsColumns:settings[0].slice(12).split(',').length,settingsBlocks:settings.length,settingsBytes:settings.map(x=>Buffer.byteLength(x+'\n')),csvWithoutFirmwareLegendBytes:info.size,latestSampleJsonBytes:Buffer.byteLength(JSON.stringify(w.eval('W.K.map(k=>k.savedSample)')))}));

 const invalid=await req('/benchlog/add','POST','not newline terminated');check(invalid.status===400,'simulator rejects malformed CSV appends');
 await req('/benchlog/clear','POST');check((await (await req('/benchlog?i=1')).json()).size===0,'clear really empties simulated CSV storage');
 console.log('ALL '+count+' PREVIEW BENCH INTEGRATION TESTS PASSED');
 }finally{if(dom)dom.window.close();child.kill();}
})().catch(e=>{console.error(e);process.exitCode=1;});
