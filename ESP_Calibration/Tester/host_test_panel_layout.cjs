/* [EN] Real Chromium layout/scroll tests; optional browser tooling is not firmware. */
'use strict';
const fs=require('fs'),path=require('path'),{spawn}=require('child_process');
let chromium,engine;
try{({chromium}=require('playwright-core'));if(!process.env.CHROMIUM_PATH)engine=require('@sparticuz/chromium');}
catch(e){console.log('SKIP: install playwright-core and @sparticuz/chromium (or set CHROMIUM_PATH); browser layout tests did not run.');process.exit(0);}
const pause=ms=>new Promise(r=>setTimeout(r,ms));let checks=0,failures=0;
function check(ok,msg,detail){++checks;if(ok)console.log('  ok   '+msg);else{++failures;console.error('  FAIL '+msg+' '+JSON.stringify(detail||''));}}
(async()=>{
 const server=spawn(process.execPath,[path.join(__dirname,'../../tools/panel_preview_server.js')],{env:{...process.env,PORT:'0',PREVIEW_BENCH_DEMO:'0'},stdio:['ignore','pipe','pipe']});let browser;
 try{
 const base=await new Promise((resolve,reject)=>{let logs='';const t=setTimeout(()=>reject(Error('preview startup timeout')),10000);server.stdout.on('data',buf=>{logs+=buf;const m=logs.match(/http:\/\/0\.0\.0\.0:(\d+)/);if(m){clearTimeout(t);resolve('http://127.0.0.1:'+m[1]);}});server.once('error',reject);server.once('exit',code=>reject(Error('server exit '+code)));});
 const launch={headless:true};
 if(engine){
  const lib=path.join(await engine.inflate(path.join(path.dirname(require.resolve('@sparticuz/chromium')),'../bin/al2023.tar.br')),'lib');
  launch.executablePath=await engine.default.executablePath();launch.args=engine.default.args;
  launch.env={...process.env,LD_LIBRARY_PATH:lib+':'+(process.env.LD_LIBRARY_PATH||'')};
 }else{launch.executablePath=process.env.CHROMIUM_PATH;launch.args=['--no-sandbox'];}
 browser=await chromium.launch(launch);
 const page=await browser.newPage();await page.addInitScript(()=>{window.setInterval=()=>0;const timeout=window.setTimeout;window.setTimeout=(fn,ms,...args)=>fn?.name==='poll'?0:timeout(fn,ms,...args);});
 const src=fs.readFileSync(process.env.PANEL_LAYOUT_SOURCE||path.join(__dirname,'../plink_panel.h'),'utf8');
 const html=src.split('R"HTML(')[1].split(')HTML"')[0];
 await page.route(base+'/',route=>route.fulfill({contentType:'text/html',body:html}));
 const hit=async(locator)=>locator.evaluate(e=>{const r=e.getBoundingClientRect();for(const fy of [.5,.7,.3])for(const fx of [.5,.8,.2,.95,.05]){const x=r.left+r.width*fx,y=r.top+r.height*fy,t=document.elementFromPoint(x,y);if(t&&(t===e||e.contains(t)))return {x,y};}return null;});
 const clickLabel=async(locator)=>{await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));const p=await hit(locator);if(!p)throw Error('No visible hit point '+JSON.stringify(await locator.evaluate(e=>{const r=e.getBoundingClientRect();return {id:e.dataset.i,connected:e.isConnected,html:e.outerHTML,r:r.toJSON(),hit:document.elementFromPoint(r.x+r.width/2,r.y+r.height/2)?.outerHTML.slice(0,500)};}))); await page.mouse.click(p.x,p.y);};
 const fit=async(label)=>{
  await page.waitForTimeout(120);
  const d=await page.evaluate(()=>({w:innerWidth,sw:document.documentElement.scrollWidth,ports:[...document.querySelectorAll('.srvw,.tw,.qgm,.evtlist,.calmodalbody')].filter(e=>e.clientWidth&&e.scrollWidth>e.clientWidth+1).map(e=>({cl:e.className,cw:e.clientWidth,sw:e.scrollWidth}))}));
  check(d.sw<=d.w+1&&!d.ports.length,label+' fits width without a horizontal scrollport',d);
 };
 for(const width of [320,375,760,1024,1440]){
  await page.setViewportSize({width,height:720});await page.goto(base);await page.waitForFunction(()=>window.D&&D.p);await page.evaluate(()=>document.fonts.ready);await page.waitForTimeout(200);
  await page.locator('nav [data-t="0"]').click();await fit(width+' status');
  for(const n of [1,2]){
   const label=page.locator('#dutyedit'+n);await label.evaluate(e=>scrollTo(0,e.getBoundingClientRect().top+scrollY-650));await page.waitForTimeout(30);
   const before=await page.evaluate(()=>scrollY);await clickLabel(label);
   const p=await page.evaluate(()=>{const r=document.getElementById('evpop')?.getBoundingClientRect();return {y:scrollY,top:r?.top,bottom:r?.bottom,left:r?.left,right:r?.right};});
   check(p.y===before&&p.top>=0&&p.bottom<=720&&p.left>=0&&p.right<=width+1,width+' duty '+n+' editor fits the viewport without scrolling',p);
   await page.keyboard.press('Escape');check(await page.evaluate(()=>scrollY)===before,width+' duty '+n+' cancel preserves scroll');
  }

  await page.locator('nav [data-t="2"]').click();
  for(let sub=0;sub<5;sub++){
   await page.locator('#sbt [data-s="'+sub+'"]').click();
   if(sub===1){for(let u=1;u<=8;u++){await page.locator('#usel [data-u="'+u+'"]').click();await fit(width+' scenario '+u);}}
   else await fit(width+' settings '+sub);
  }
  await page.locator('#sbt [data-s="0"]').click();
  await page.evaluate(()=>qgraph());
  const graphLabels=page.locator('.qgm [data-i]');let graphLabel=null;
  // Some existing chart annotations overlap; use an actually exposed label,
  // not a forced click through a different annotation or a detached redraw.
  for(let i=await graphLabels.count()-1;i>=0;i--){const l=graphLabels.nth(i);await l.evaluate(e=>scrollTo(0,e.getBoundingClientRect().top+scrollY-650));await page.waitForTimeout(120);if(await hit(l)){graphLabel=l;break;}}
  if(!graphLabel)throw Error('No exposed chart label');
  for(const label of [graphLabel,page.locator('.qgcm [data-i]:visible').nth(0),page.locator('.qgcm [data-i]:visible').nth(4)]){
   await label.evaluate(e=>scrollTo(0,e.getBoundingClientRect().top+scrollY-650));await page.waitForTimeout(120);
   const r=await label.boundingBox();if(!r)throw Error('Missing editable label '+JSON.stringify(await label.evaluate(e=>({html:e.outerHTML,tab:TAB,sub:STAB,cl:e.parentElement.outerHTML.slice(0,400),box:e.getBoundingClientRect().toJSON()}))));if(r.y<160||r.y>700){await label.evaluate(e=>e.scrollIntoView({block:'center'}));await page.waitForTimeout(120);}
   const before=await page.evaluate(()=>scrollY);
   await clickLabel(label);await page.waitForTimeout(25);
   const opened=await page.evaluate(()=>{const p=document.getElementById('evpop'),r=p&&p.getBoundingClientRect();return {y:scrollY,top:r&&r.top,bottom:r&&r.bottom,left:r&&r.left,right:r&&r.right,focus:document.activeElement===p?.querySelector('input'),sw:document.documentElement.scrollWidth};});
   check(opened.y===before&&opened.focus&&opened.top>=0&&opened.bottom<=720&&opened.left>=0&&opened.right<=width+1&&opened.sw<=width+1,width+' low-edge editor opens in view without moving the page', {before,opened});
   await page.evaluate(()=>draw(D));check(await page.evaluate(()=>document.activeElement===document.querySelector('#evpop input')),width+' telemetry draw preserves the open editor focus');
   await page.keyboard.press('Escape');await page.waitForTimeout(20);check(await page.evaluate(()=>scrollY)===before,width+' cancel leaves scroll position unchanged');
  }
  // Switching labels without editing must not replace the clicked SVG on blur.
  // Chip-to-chip navigation also exercises the shared delegated editor.
  const firstChip=page.locator('.qgcm [data-i]:visible').first();await firstChip.evaluate(e=>e.scrollIntoView({block:'center'}));await page.waitForTimeout(120);await clickLabel(firstChip);
  const labels=page.locator('.qgcm [data-i]:visible');
  let next=null,id=null;for(let i=1;i<await labels.count();i++){const p=await hit(labels.nth(i));if(p&&p.y>160&&p.y<700){next=p;id=await labels.nth(i).getAttribute('data-i');break;}}
  if(!next)throw Error('No second visible label');await page.mouse.click(next.x,next.y);
  check(await page.evaluate(id=>EVOPEN===+id&&document.activeElement===document.querySelector('#evpop input'),id),width+' direct label-to-label click opens the second editor');
  await page.keyboard.press('Escape');
  await page.locator('nav [data-t="1"]').click();
  await page.evaluate(()=>{CALS=[];for(let i=0;i<6;i++)CALS.push({sc:'BOTH',d:i*5,use:1,r1:80+i*20,r2:90+i*20,b1:i*30,b2:i*31,ts:100+i});wbuild(['BAT1','BAT2','BOTH'],[0,5]);calsmp();calrun();});
  await fit(width+' populated capture and sample tables');
  await page.evaluate(()=>calmodalOpen());await fit(width+' calibration confirmation');await page.evaluate(()=>calmodalClose());
  if(width<=760){check(await page.evaluate(()=>[...document.querySelectorAll('#wT td,#calsl td')].every(e=>e.dataset.h)),width+' stacked dynamic tables retain every column heading');}
  await page.evaluate(()=>{wform(0,[1,2]);});
  await fit(width+' active two-battery capture form');
  // Resolve the form to clean up its keyboard/timer state before navigating.
  await page.evaluate(()=>{W.choose&&W.choose({a:'end'});});await pause(10);
 }
 console.log(checks+' BROWSER LAYOUT CHECKS; '+failures+' FAILURE(S)');if(failures)process.exitCode=1;
 }finally{if(browser)await browser.close();server.kill();}
})().catch(e=>{console.error(e);process.exitCode=1;});
