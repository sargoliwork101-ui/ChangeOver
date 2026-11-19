'use strict';
// Exercise demo-only review thresholds and verify isolation from shipped panel assets.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const {JSDOM}=require('jsdom');
const root=path.join(__dirname,'..');
const script=fs.readFileSync(path.join(root,'Simulator','battery_wear_preview.js'),'utf8');
const panel=fs.readFileSync(path.join(root,'simple_preview.html'),'utf8');
let checks=0;
function check(ok,label){assert.ok(ok,label);checks++;console.log('PASS: '+label);}
(async()=>{
 const dom=new JSDOM('<!doctype html><html lang="fa" dir="rtl"><head></head><body><div id="pg1"><div id="existing">existing chart</div></div></body></html>',{runScripts:'outside-only'}),w=dom.window,d=w.document;
 let tab=-1,requests=0;w.showTab=value=>{tab=value;};w.fetch=()=>{requests++;throw Error('demo must not fetch');};
 await new Promise(r=>setImmediate(r));w.eval(script);
 check(tab===1&&d.getElementById('pg1').firstElementChild.id==='wearDemo','demo indicator precedes the existing charts and opens charts tab');
 check(d.querySelector('.wear-badge').textContent.includes('دادهٔ آزمایشی')&&d.getElementById('wearDemo').textContent.includes('درصد سلامت یا تعداد چرخهٔ کامل'),'mock data and the difference from health/full cycles are explicitly labeled');
 const statuses=()=>[...d.querySelectorAll('.wear-state')].map(el=>el.textContent);
 check(statuses()[0]==='نزدیک حد فرضی بررسی'&&statuses()[1]==='نیاز به بررسی باتری','independent battery inputs produce different review statuses');
 for(const [preset,message] of [['low','زیر حد فرضی بررسی'],['near','نزدیک حد فرضی بررسی'],['over','نیاز به بررسی باتری'],['unknown','شمارنده موجود نیست']]){
  d.querySelector('[data-wear-preset='+preset+']').click();check(statuses().every(value=>value===message),'preset '+preset+' renders the expected state for both rows');
 }
 check([...d.querySelectorAll('.wear-dot')].every(el=>el.hidden)&&[...d.querySelectorAll('.wear-count')].every(el=>el.textContent==='تعداد نامعلوم'),'missing counters show unknown rather than zero or a healthy verdict');
 d.querySelector('[data-wear-preset=near]').click();const upper=d.getElementById('wearUpper'),limit=d.getElementById('wearLimit');
 for(const [value,message] of [[239,'زیر حد فرضی بررسی'],[240,'نزدیک حد فرضی بررسی'],[299,'نزدیک حد فرضی بررسی'],[300,'نیاز به بررسی باتری'],[900,'نیاز به بررسی باتری']]){
  upper.value=value;upper.dispatchEvent(new w.Event('input'));check(statuses()[0]===message,'exact illustrative threshold at '+value);
 }
 check(d.querySelector('[data-wear=upper]').style.getPropertyValue('--wear-progress')==='100%','over-limit progress is bounded at the end of the line');
 for(const value of ['',0,-1,1.5,100001]){
  limit.value=value;limit.dispatchEvent(new w.Event('input'));check(statuses().every(value=>value==='حد بررسی مشخص نشده'),'invalid illustrative limit does not produce a verdict: '+value);
 }
 limit.value=300;const known=d.getElementById('wearHasLimit');known.checked=false;known.dispatchEvent(new w.Event('input'));
 check(statuses().every(value=>value==='حد بررسی مشخص نشده'),'unknown threshold preserves counts without assigning a wear status');
 check([...d.querySelectorAll('.wear-track')].every(el=>el.getAttribute('aria-label').includes('بدون حد بررسی')),'nonvisual labels convey missing limits and counters');
 w.eval(script);check(d.querySelectorAll('#wearDemo').length===1,'reinitialization cannot duplicate the prototype');
 check(requests===0&&!/localStorage|sessionStorage|fetch\(/.test(script),'prototype never sends requests or changes stored counters/settings');
 w.close();
 for(const file of ['simple_panel.h','simple_preview.html','index.html','simple_panel_clean.html','ESP_USER.ino']){
  const content=fs.readFileSync(path.join(root,file),'utf8');check(!content.includes('wearDemo')&&!content.includes('battery_wear_preview'),'demo controls and mock thresholds are absent from shipped '+file);
 }
 check(!panel.includes(script),'normal offline preview does not embed the wear mockup');
 console.log(`ALL ${checks} BATTERY WEAR PREVIEW CHECKS PASSED`);
})().catch(e=>{console.error(e);process.exitCode=1;});
