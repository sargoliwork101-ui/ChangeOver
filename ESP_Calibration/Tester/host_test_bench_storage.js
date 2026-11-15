/* [EN] Frozen before/after bench storage contract: signs, decimals and field order. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom missing; bench storage contract did not run.');process.exit(0);}
const timers=[];let checks=0;
function check(ok,msg){assert.ok(ok,msg);++checks;console.log('  ok   '+msg);}
const source=process.env.PANEL_BENCH_BASELINE||path.join(__dirname,'../panel_preview.html');
(async()=>{
 const dom=new JSDOM(fs.readFileSync(source,'utf8'),{runScripts:'dangerously',pretendToBeVisual:true,url:'http://localhost/',beforeParse(w){w.scrollTo=()=>{};const set=w.setInterval.bind(w);w.setInterval=(f,t)=>{const id=set(f,t);timers.push(id);return id;};}});
 const w=dom.window;
 try{
 await new Promise(r=>setTimeout(r,80));timers.forEach(id=>w.clearInterval(id));
 w.D={on:1,p:Array.from({length:171},(_,i)=>i),t:Array(30).fill(0)};
 const m={a:i=>100+i+0.125,lo:Array.from({length:30},(_,i)=>10+i),hi:Array.from({length:30},(_,i)=>200+i),la:Array.from({length:30},(_,i)=>300+i),seq:65535,fl:39,or:128};
 const row=w.wrow('BOTH',2,250,0,1234,m,{ii:-50.25,b1:-0.5,b2:100.125,note:'audit'},'2026-11-01T12:00:00.000Z');
 const expected=['BOTH',3,250,0,1234,'2026-11-01T12:00:00.000Z',
 '100.1',10,200,101,102,12,202,103,13,203,104,14,204,305,306,
 '107.1',17,207,108,109,19,209,110,20,210,111,21,211,312,313,
 65535,39,114,115,116,117,118,128,320,321,322,323,324,-50.25,-0.5,100.125,'audit'].join(',')+'\n';
 check(row===expected,'all 53 existing CSV fields retain exact order, signs, decimals and formatting');
 const missing=w.wrow('BAT1',0,0,0,0,m,{ii:0,b1:0},'date');
 check(missing.endsWith(',0,0,-,-\n'),'missing inactive-channel readings stay distinct from measured zero');
 check(w.wset()===Array.from({length:171},(_,i)=>i).join(','),'settings preserve all 171 values by wire ID');
 w.eval('CALS=[]');const id=w.calpush(m,{b1:-0.5,b2:100.125},[1,2],'BOTH',25);
 const sample=w.CALS[id],parsed=JSON.parse(JSON.stringify(sample));
 check(sample.r1===100.125&&sample.r2===107.125&&sample.b1===-0.5&&sample.b2===100.125,'fit samples retain unrounded raw means and signed fractional DMM readings');
 check(JSON.stringify(w.calnorm(parsed))===JSON.stringify(w.calnorm(sample)),'export/import normalization preserves all sample information');
 let writes=[];w.wlog=async text=>writes.push(text);await w.wsync();await w.wsync();
 check(writes.length===1,'unchanged settings are not repeatedly written within one scenario');
 w.D.p[16]=250;await w.wsync();
 check(writes.length===2&&writes[1].includes(',250,'),'a changed setting currently writes a new complete snapshot (baseline behavior)');
 console.log('ALL '+checks+' BENCH STORAGE CONTRACT TESTS PASSED');
 }finally{dom.window.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
