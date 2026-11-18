'use strict';
// Execute the firmware renderer with independent board-supplied percentage maps.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const {JSDOM}=require('jsdom');
const source=fs.readFileSync(path.join(__dirname,'../simple_panel.h'),'utf8');
const html=source.split('R"rawliteral(')[1].split(')rawliteral"')[0];
let checks=0,failures=0;
function check(ok,message){checks++;try{assert.ok(ok,message);console.log('PASS: '+message);}catch(e){failures++;console.error('FAIL: '+message);}}
function session(){
 const dom=new JSDOM(html,{runScripts:'outside-only',url:'http://panel.test/'}),w=dom.window;
 w.setInterval=()=>0;w.setTimeout=()=>0;w.addEventListener=()=>{};w.fetch=async()=>{throw Error('offline');};
 w.eval(html.match(/<script>([\s\S]*?)<\/script>/)[1]);
 return {dom,w,render({dis=[21000,27000],chg=[21000,29000],v=27000,low=v/2,high=v/2,input=false,state=0}={}){
  const t=Array(31).fill(0);t[14]=input?24000:0;t[15]=v;t[17]=low;t[18]=high;t[6]=t[13]=state;
  const data={on:1,age:0,ce:0,n:1,t,fl:input?2:0,fl2:0,inputPresent:input?1:0,
   pv:[1000,10,1000,150,...dis,100,95,5,2,2,3,...chg,21000,28000,500,500,1000]};
  w.render(data);return data;
 }};
}
for(const [label,args,want] of [
 ['discharge uses 74/75',{input:false},[100,100,100]],
 ['charge uses 119/120 for pack AND both batteries',{input:true},[75,75,75]],
 ['lower charge ceiling never clips discharge',{dis:[21000,29000],chg:[21000,27000],v:29000},[100,100,100]],
 ['charge uses its own lower limit',{dis:[20000,28000],chg:[22000,30000],v:24000,input:true},[25,25,25]],
 ['discharge uses its own lower limit',{dis:[20000,28000],chg:[22000,30000],v:24000},[50,50,50]],
 ['unequal batteries use independent measured voltages',{dis:[21000,27000],chg:[23000,29000],v:26000,low:12500,high:13500,input:true},[50,33,66]],
 ['charge zero bound',{v:21000,input:true},[0,0,0]],
 ['charge saturation',{v:31000,input:true},[100,100,100]],
 ['discharge zero bound',{v:20000},[0,0,0]]
]){
 const s=session();try{
  const data=s.render(args);
  check(JSON.stringify([data.pctPack,data.pctLowStable,data.pctHighStable])===JSON.stringify(want),label);
  check(s.w.document.getElementById('batPct').textContent===want[0]+'%',label+' reaches the displayed pack number');
 }finally{s.dom.window.close();}
}
{
 const s=session();try{
  s.render({input:true});const d=s.render({input:false});
  check(d.pctPack===100&&d.pctLowStable===100&&d.pctHighStable===100,'input loss switches all three numbers to the discharge map');
  const c=s.render({input:true});
  check(c.pctPack===75&&c.pctLowStable===75&&c.pctHighStable===75,'input return restores the separate charge map');
  const edited=s.render({input:true,chg:[21000,27000]});
  check(edited.pctPack===100&&edited.pctLowStable===100&&edited.pctHighStable===100,'fresh board charge bounds update the three numbers');
  const raw=s.w.eval('vToPctPack(27000)');s.render({input:true,dis:[21000,29000],chg:[21000,27000]});
  check(raw===100&&s.w.eval('vToPctPack(27000)')===75&&s.w.eval('vToPctPackChg(27000)')===100,'a new discharge bound does not overwrite the charge bound');
 }finally{s.dom.window.close();}
}
{
 const s=session();try{
  const d=s.render({input:true,state:3});
  check(d.pctPack===75&&s.w.document.getElementById('statusTitle').textContent==='شارژ کامل','FLOAT completion remains independent of the voltage percentage');
  const c=s.render({input:true,state:1});
  check(c.isCharging&&s.w.document.getElementById('batPct').style.display==='none','active charging keeps the existing hidden-number presentation');
 }finally{s.dom.window.close();}
}
// The generated preview must not tie its charge sliders to discharge sliders.
(async()=>{
 const preview=fs.readFileSync(path.join(__dirname,'../simple_preview.html'),'utf8');
 const {VirtualConsole}=require('jsdom');
 const errors=[],vc=new VirtualConsole();vc.on('jsdomError',e=>errors.push(e.message));
 const dom=new JSDOM(preview,{runScripts:'dangerously',url:'file:///panel.html',virtualConsole:vc,
  beforeParse(w){w.HTMLCanvasElement.prototype.getContext=()=>null;w.setInterval=()=>0;}});
 try{
  const w=dom.window;
  for(let i=0;i<6;i++)await new Promise(r=>setImmediate(r));
  const sliders=[...w.document.querySelectorAll('#pg3 .simrow')];
  function setSlider(label,value){
   const row=sliders.find(e=>e.textContent.includes(label));
   assert.ok(row,'preview slider '+label);const input=row.querySelector('input');
   input.value=value;input.dispatchEvent(new w.Event('input',{bubbles:true}));
  }
  setSlider('۱۰۰٪ دشارژ',27000);setSlider('۱۰۰٪ شارژ',30000);
  let data=await (await w.fetch('/t')).json();
  check(data.pv[5]===27000&&data.pv[13]===30000,'native preview sliders retain independent discharge and charge ceilings');
  setSlider('۱۰۰٪ دشارژ',28000);
  data=await (await w.fetch('/t')).json();
  check(data.pv[5]===28000&&data.pv[13]===30000,'editing the preview discharge ceiling leaves charge unchanged');
  await w.fetch('/sim?p119=23000&p120=29000',{method:'POST'});
  data=await (await w.fetch('/t')).json();
  check(data.pv[4]===21000&&data.pv[5]===28000&&data.pv[12]===23000&&data.pv[13]===29000,'offline simulator parameter route accepts separate charge bounds');
  check(!errors.length,'independent preview controls produce no browser script errors');
 }finally{dom.window.close();}
})().catch(e=>{failures++;console.error(e);}).finally(()=>{
 console.log(`${checks} BATTERY PERCENT CHECKS; ${failures} FAILURES`);
 if(failures)process.exitCode=1;
});
