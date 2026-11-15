/* [EN] Compact bench controls preserve accessible names, discoverable help and action bindings. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom missing; bench-icon DOM tests did not run.');process.exit(0);}
const intervals=[];let checks=0;
function check(value,msg){assert.ok(value,msg);++checks;console.log('  ok   '+msg);}
(async()=>{
 const dom=new JSDOM(fs.readFileSync(process.env.PANEL_ICON_BASELINE||path.join(__dirname,'../panel_preview.html'),'utf8'),{runScripts:'dangerously',pretendToBeVisual:true,url:'http://localhost/',beforeParse(w){w.scrollTo=()=>{};const set=w.setInterval.bind(w);w.setInterval=(f,t)=>{const id=set(f,t);intervals.push(id);return id;};}});
 const w=dom.window,d=w.document;
 try{
 await new Promise(r=>setTimeout(r,80));intervals.forEach(id=>w.clearInterval(id));
 const icons=[...d.querySelectorAll('#p1 .ibtn')];
 check(icons.length>=9,'routine bench actions are compact icons instead of long repeated button captions');
 check(icons.every(e=>e.getAttribute('aria-label')&&e.title&&/[\u0600-\u06ff]/.test(e.getAttribute('aria-label'))),'every icon has a Persian accessible action name and a hover title');
 check(icons.every(e=>e.tagName==='BUTTON'||(e.getAttribute('role')==='button'&&e.tabIndex===0)),'non-button file picker participates in keyboard focus order');
 check(icons.every(e=>parseFloat(w.getComputedStyle(e).minHeight)>=44&&parseFloat(w.getComputedStyle(e).minWidth)>=44),'icon hit targets remain at least 44 by 44 CSS pixels');
 check(icons.every(e=>[...e.textContent.trim()].length<=3),'icon-only captions stay visually compact');
 const legend=d.getElementById('benchIconHelp');
 check(legend&&!legend.open&&legend.textContent.includes('∑')&&legend.textContent.includes('⧉')&&legend.textContent.includes('خواندن جدول'),'touch users have an on-demand icon legend, not hover-only explanations');
 legend.querySelector('summary').click();check(legend.open,'native help summary opens with ordinary activation');legend.querySelector('summary').click();check(!legend.open,'help can be collapsed again without changing controls');
 check(d.querySelectorAll('#p1 details.bhelp:not([open])').length>=4,'long static instructions are collapsed by default but retained in help');
 check(['calck','calsl','calst','calpoints','lutcmp','wS0'].every(id=>!d.getElementById(id).closest('details')),'live data, warnings, checklist and computed results are never hidden inside help');
 const writeIds=['lbtnS1','lbtnS2','lbtnSA','lbtnR'];
 check(writeIds.every(id=>/[\u0600-\u06ff]/.test(d.getElementById(id).textContent)&&d.getElementById(id).textContent.trim().length>3&&d.getElementById(id).getAttribute('aria-label')),'board write/reset actions retain short visible labels, not ambiguous icon-only controls');
 check(writeIds.every(id=>d.getElementById(id).disabled),'initial board readback/write/reset locks are retained');
 check([...d.querySelectorAll('#p1 [onclick="calclr()"],#p1 [onclick="wclear()"]')].every(e=>e.textContent.trim().length>3&&e.getAttribute('aria-label')),'destructive actions retain visible text and explicit action names');
 let calls=0;const original=w.calrun;w.calrun=()=>{++calls;};d.querySelector('#p1 [onclick="calrun()"]').click();w.calrun=original;
 check(calls===1,'compact calculate icon still invokes the same calculation exactly once');
 let downloads=0;const save=w.wdl;w.wdl=()=>{++downloads;};d.querySelector('#p1 [onclick="wdl()"]').click();w.wdl=save;
 check(downloads===1,'compact log download invokes its original filename-confirmed dispatcher');
 const picker=d.getElementById('calf'),label=picker.closest('[role="button"]');let selections=0;picker.click=()=>{++selections;};
 for(const key of ['Enter',' ']){const event=new w.KeyboardEvent('keydown',{key,bubbles:true,cancelable:true});label.dispatchEvent(event);check(event.defaultPrevented,'file-picker keyboard activation prevents page scroll/default handling');}
 check(selections===2,'Enter and Space each open the sample file picker exactly once');
 let copies=0;const copy=w.calcopy;w.calcopy=()=>{++copies;};d.querySelector('#p1 [onclick="calcopy()"]').click();w.calcopy=copy;
 check(copies===1,'copy icon remains distinct from build-code and download-code actions');
 w.D={on:1,fl:32,p:Array(171).fill(0),t:Array(31).fill(0)};w.eval('W={run:true,act:[1],man:true};');w.wbuild(['BAT1'],[0]);
 const form=w.wform(0,[1]);
 check(d.getElementById('wGo').textContent==='✓ ثبت'&&d.getElementById('wGo').getAttribute('aria-label')==='ثبت و مرحلهٔ بعد','capture submit is short but retains its complete action name');
 check(d.getElementById('wRe').classList.contains('ibtn')&&d.getElementById('wRe').title==='تکرار همین مرحله','repeat uses the same icon pattern while retaining its exact meaning');
 check(d.getElementById('wEn').textContent.includes('پایان')&&d.getElementById('wB1')&&!d.getElementById('wIi'),'end action and active-battery measurement inputs remain visible and unchanged');
 d.getElementById('wRe').click();check((await form).a==='repeat','compact repeat settles the same capture choice and cleans up its form');
 w.eval('W.run=false');
 check(!d.getElementById('wGo')&&w.eval('W.choose===null'),'dynamic form icons leave no stale handlers/choices after closing');
 console.log('ALL '+checks+' BENCH ICON TESTS PASSED');
 }finally{dom.window.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
