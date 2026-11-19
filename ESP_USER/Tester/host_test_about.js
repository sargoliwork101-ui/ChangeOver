'use strict';
// Check the compiled About source, shared footer, retry behavior and offline preview.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const {JSDOM,VirtualConsole}=require('jsdom');
const root=path.join(__dirname,'..');
const source=fs.readFileSync(path.join(root,'simple_panel.h'),'utf8');
const html=source.split('R"rawliteral(')[1].split(')rawliteral"')[0];
const about=fs.readFileSync(path.join(root,'about_content.h'),'utf8').split('R"about(')[1].split(')about"')[0];
const tick=async()=>{for(let i=0;i<5;i++)await new Promise(r=>setImmediate(r));};
let checks=0;
function check(ok,label){assert.ok(ok,label);checks++;console.log('PASS: '+label);}
(async()=>{
 const dom=new JSDOM(html,{runScripts:'outside-only',pretendToBeVisual:true,url:'http://panel.test'}),w=dom.window,d=w.document;
 w.setInterval=()=>0;w.addEventListener=()=>{};w.fetch=async()=>{throw Error('offline');};w.HTMLCanvasElement.prototype.getContext=()=>null;
 w.eval(html.match(/<script>([\s\S]*?)<\/script>/)[1]);await tick();
 const el=d.getElementById('about');
 check(d.querySelectorAll('#about').length===1&&!el.closest('.pgx')&&el.nextElementSibling.classList.contains('footer'),'one About menu sits below all tabs, above the version footer');
 check(el.querySelector('summary').textContent==='درباره ما'&&el.getAttribute('ontoggle')==='loadAbout(this)','native keyboard-accessible disclosure invokes the real loader');
 let calls=0;w.fetch=async url=>{if(url!=='/about')throw Error('offline');calls++;return {ok:true,text:async()=>about};};
 await w.loadAbout(el);check(calls===0,'closed menu makes no request');
 el.open=true;await w.loadAbout(el);
 check(d.getElementById('aboutBody').innerHTML===about&&el.dataset.loaded==='1','About HTML comes from the independent compiled content file');
 const rows=el.querySelectorAll('p');
 check(rows.length===3&&rows[0].querySelector('bdi').textContent==='09367969890'&&rows[1].querySelector('bdi').textContent===''&&rows[2].querySelector('span').textContent==='تهران-تهران','requested phone, empty website and office address are exact');
 check(!source.includes('09367969890')&&!source.includes('تهران-تهران'),'contact values are not duplicated in the application page');
 for(const tab of [0,1,2]){w.showTab(tab);check(!el.closest('.pgx')&&!el.hidden,'About remains available on tab '+tab);}
 await w.loadAbout(el);el.open=false;await w.loadAbout(el);el.open=true;await w.loadAbout(el);check(calls===1,'reopening an already-loaded menu does not refetch');
 delete el.dataset.loaded;w.fetch=async()=>({ok:false});await w.loadAbout(el);
 check(!el.dataset.loaded&&!el.dataset.busy&&d.getElementById('aboutBody').textContent.includes('ناموفق'),'failed response allows retry and shows an understandable error');
 w.fetch=async()=>({ok:true,text:async()=>about});await w.loadAbout(el);check(el.dataset.loaded==='1','retry replaces the error with the contact content');
 delete el.dataset.loaded;let resolve;calls=0;w.fetch=()=>{calls++;return new Promise(r=>resolve=r);};const pending=w.loadAbout(el);await w.loadAbout(el);
 check(calls===1&&el.dataset.busy==='1','rapid toggles cannot duplicate an in-flight request');resolve({ok:true,text:async()=>about});await pending;w.close();
 const errors=[],vc=new VirtualConsole();vc.on('jsdomError',e=>errors.push(e.message));
 const offline=new JSDOM(fs.readFileSync(path.join(root,'simple_preview.html'),'utf8'),{runScripts:'dangerously',pretendToBeVisual:true,url:'file:///panel.html',virtualConsole:vc,beforeParse(win){win.setInterval=()=>0;win.HTMLCanvasElement.prototype.getContext=()=>null;}});
 await tick();const menu=offline.window.document.getElementById('about');menu.open=true;await offline.window.loadAbout(menu);
 check(menu.querySelector('#aboutBody').innerHTML===about,'offline simulator uses the same separate contact source');
 check(!errors.length,'About preview produces no script errors');offline.window.close();
 console.log(`ALL ${checks} ABOUT CHECKS PASSED`);
})().catch(e=>{console.error(e);process.exit(1);});
