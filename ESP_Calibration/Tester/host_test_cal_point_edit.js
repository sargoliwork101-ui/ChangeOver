/* [EN] Generated-point deletion uses the real panel builders and fit, no cloned algorithm. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom not installed; point-edit tests did not run.');process.exit(0);}
const timers=[];let checks=0;
function check(v,msg){assert.ok(v,msg);++checks;console.log('  ok   '+msg);}
const dom=new JSDOM(fs.readFileSync(path.join(__dirname,'../panel_preview.html'),'utf8'),{runScripts:'dangerously',pretendToBeVisual:true,url:'http://localhost/',beforeParse(w){w.scrollTo=()=>{};const set=w.setInterval.bind(w);w.setInterval=(f,t)=>{const id=set(f,t);timers.push(id);return id;};}});
const w=dom.window,d=w.document;
(async()=>{
 await new Promise(r=>setTimeout(r,80));
 try{
 timers.forEach(id=>w.clearInterval(id));w.D={on:1,fl:7,p:Array(171).fill(0),t:Array(30).fill(0)};w.D.p[2]=w.D.p[3]=1000;
 const seed=(bad=false)=>{w.eval('CALS=[];');for(let i=0;i<7;i++)w.CALS.push({sc:'BOTH',d:i*5,use:1,r1:80+i*20,r2:90+i*20,b1:i?i*26-20:-20,b2:i?i*27-15:-15,ts:i});w.CALS.push({...w.CALS[3],b1:59});if(bad)w.CALS[4].b1=700;w.calinvalidate();w.calrun();};
 const table=n=>w.calbuild(n),cut=()=>w.eval('CALCUT');
 seed();const original=JSON.stringify(w.CALS),second=JSON.stringify(table(2));
 check(table(1).X.length===7&&w.CALS.length===8,'eight samples merge to seven generated points without deleting a sample');
 check(d.querySelectorAll('#calpoints table').length===2,'Calculate displays generated tables and row deletion controls for both channels');
 const deleteButton=[...d.querySelectorAll('#calpoints button')].find(b=>b.getAttribute('onclick')==='caldrop(1,140)');deleteButton.click();
 check(!table(1).X.includes(140)&&table(1).X.length===6,'clicking a generated merged point removes it from the proposed table');
 check(w.calsel(1).length===6,'all samples merged at the deleted count are excluded from that channel fit');
 check(JSON.stringify(w.CALS)===original&&JSON.stringify(table(2))===second,'source samples and the other channel are untouched');
 w.caldrop(1,180);w.caldrop(1,200);
 check(table(1).X.length===4&&cut()[0].length===3,'several generated points can be removed cumulatively');
 w.calrun();check(table(1).X.length===4,'recalculate preserves explicit point deletions');
 const codeBefore=JSON.stringify(cut());w.caldrop(1,80);
 check(JSON.stringify(cut())===codeBefore,'the measured duty-zero group cannot be deleted even by a direct function call');
 const pack=w.lpack(0,{ready:true,T:[{X:[],Y:[]},{X:[],Y:[]}]});
 check(!pack.bad&&!pack.T[0].X.includes(140)&&pack.T[1].X.length===7,'transmission payload is rebuilt from edited points, preserving the other battery');
 w.calcode();const code=d.getElementById('calcd').value;
 check(code.includes('80u, 100u, 120u, 160u')&&!code.includes('80u, 100u, 120u, 140u'),'generated firmware table also honors deletions');
 w.caldrop(1,null);check(!d.getElementById('calcd').value,'editing points clears stale generated code so it cannot be copied or downloaded');check(table(1).X.length===7&&cut()[0].length===0,'Restore returns all channel points without importing or measuring again');
 seed(true);check(!!table(1).bad&&table(2).bad==='','a nonmonotonic suspect sample blocks only its table rather than being silently removed');
 w.caldrop(1,160);check(!table(1).bad&&table(1).X.length===6,'deleting the intentionally suspect point repairs monotonicity');
 w.eval('CAL_BUSY=true');const snapshot=JSON.stringify(cut());w.caldrop(1,180);check(JSON.stringify(cut())===snapshot,'point edits are locked during a calibration transaction');w.eval('CAL_BUSY=false');
 w.eval('W.run=true');w.caldrop(1,180);check(JSON.stringify(cut())===snapshot,'point edits are locked during bench capture');w.eval('W.run=false');
 w.caldrop(1,null);[100,120,140,160,180].forEach(c=>w.caldrop(1,c));const two=JSON.stringify(cut());w.caldrop(1,200);
 check(table(1).X.length===2&&JSON.stringify(cut())===two,'deletions cannot leave fewer than two distinct points');
 w.caldrop(1,null);w.eval('LUT_PENDING={p:{},target:0};');d.getElementById('lutcmp').textContent='stale payload';w.caldrop(1,140);
 check(w.eval('LUT_PENDING===null')&&!d.getElementById('lutcmp').textContent,'editing a point cancels any previously prepared transmission payload');
 w.caluse(1,false);check(cut()[0].includes(140)&&d.querySelector('#calpoints table')&&!table(1).X.includes(100),'sample selection changes automatically recalculate while preserving explicit point exclusions');
 w.caldrop(1,NaN);check(cut()[0].length===1&&cut()[0][0]===140,'invalid point identity cannot affect an unrelated point');
 console.log('ALL '+checks+' CALIBRATION POINT EDIT TESTS PASSED');
}finally{dom.window.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
