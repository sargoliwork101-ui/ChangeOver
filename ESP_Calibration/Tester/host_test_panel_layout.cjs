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
  const d=await page.evaluate(()=>({w:innerWidth,sw:document.documentElement.scrollWidth,ports:[...document.querySelectorAll('.srvw,.tw,.qgm,.evtlist,.calmodalbody,#sres .rb,#sbst')].filter(e=>e.clientWidth&&e.scrollWidth>e.clientWidth+1).map(e=>({cl:e.className,cw:e.clientWidth,sw:e.scrollWidth}))}));
  check(d.sw<=d.w+1&&!d.ports.length,label+' fits width without a horizontal scrollport',d);
 };
 // Check resolved browser colors, including dynamically created text fields.
 const fieldColors=async(label,selector)=>{
  const result=await page.locator(selector).evaluateAll(es=>{
   const lum=c=>{const rgb=c.match(/[\d.]+/g).slice(0,3).map(Number).map(x=>{x/=255;return x<=.04045?x/12.92:((x+.055)/1.055)**2.4;});return .2126*rgb[0]+.7152*rgb[1]+.0722*rgb[2];};
   return es.filter(e=>e.getClientRects().length&&!e.disabled&&e.type!=='hidden'&&e.type!=='checkbox'&&e.type!=='file').map(e=>{const s=getComputedStyle(e),fg=lum(s.color),bg=lum(s.backgroundColor);return {id:e.id,fg:s.color,bg:s.backgroundColor,ratio:(Math.max(fg,bg)+.05)/(Math.min(fg,bg)+.05)};});
  });
  check(result.length>0&&result.every(x=>x.ratio>=4.5&&!/rgba/.test(x.bg)),label+' uses opaque readable field colors',result);
 };
 for(const width of [320,375,760,1024,1440]){
  await page.setViewportSize({width,height:720});await page.goto(base);await page.waitForFunction(()=>window.D&&D.p);await page.evaluate(()=>document.fonts.ready);await page.waitForTimeout(200);
  await page.locator('nav [data-t="0"]').click();await fit(width+' status');
  await page.evaluate(()=>{window.nameResult=undefined;pmName('benchlog.csv').then(n=>window.nameResult=n);});
  await page.locator('#pmfilename').waitFor();await fieldColors(width+' filename focused','#pmfilename');
  await page.locator('#pmfilename').fill('گزارش آزمایش');await page.locator('#pmok').focus();
  await fieldColors(width+' filename unfocused','#pmfilename');await page.locator('#pmok').click();
  check(await page.evaluate(()=>window.nameResult==='گزارش آزمایش.csv'),width+' filename stays editable and keeps its extension');

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
  await page.evaluate(()=>{$('wSw').checked=false;wmode();});
  await fieldColors(width+' manual duty text','#wL');
  await fieldColors(width+' calibration controls','input,select,textarea');

  await page.evaluate(()=>{CALS=[];for(let i=0;i<40;i++)CALS.push({sc:'BOTH',d:i,use:1,r1:80+i*20,r2:90+i*20,b1:i*30,b2:i*31,ts:i});calsmp();calrun();window.sampleNodes=[...document.querySelectorAll('#calsl [data-csample]')];});
  const sampleRow=page.locator('#calsl [data-csample="15"]');
  await sampleRow.evaluate(e=>e.scrollIntoView({block:'center'}));await page.waitForTimeout(180);
  const samplePosition=()=>page.evaluate(()=>({y:scrollY,top:document.querySelector('#calsl [data-csample="15"]').getBoundingClientRect().top}));
  const sampleBefore=await samplePosition();
  await sampleRow.locator('td').nth(2).click();await page.waitForTimeout(180);
  const rowAfter=await samplePosition();
  check(rowAfter.y===sampleBefore.y&&Math.abs(rowAfter.top-sampleBefore.top)<1,width+' sample row click leaves page and row position unchanged',{sampleBefore,rowAfter});
  check(await page.evaluate(()=>CALS[15].use===0&&sampleNodes.every((r,i)=>r===document.querySelectorAll('#calsl [data-csample]')[i])&&CALS.every((z,i)=>z.ts===i)),width+' selection preserves row nodes and sample order');
  await sampleRow.locator('input').click();await page.waitForTimeout(180);const boxAfter=await samplePosition();
  check(boxAfter.y===sampleBefore.y&&Math.abs(boxAfter.top-sampleBefore.top)<1&&await page.evaluate(()=>CALS[15].use===1),width+' direct checkbox toggles once without jumping',{sampleBefore,boxAfter});
  await sampleRow.locator('button').click();
  check(await page.evaluate(()=>CALS.length===39&&CALS[15].ts===16&&document.querySelectorAll('#calsl [data-csample]')[15]===sampleNodes[16]),width+' delete keeps surviving samples in their original order');

  await page.evaluate(()=>calmodalOpen());await fit(width+' calibration confirmation');await page.evaluate(()=>calmodalClose());
  if(width<=760){check(await page.evaluate(()=>[...document.querySelectorAll('#wT td,#calsl td')].every(e=>e.dataset.h)),width+' stacked dynamic tables retain every column heading');}
  await page.evaluate(()=>{wform(0,[1,2]);});
  await fit(width+' active two-battery capture form');
  await fieldColors(width+' capture note','#wN');
  // Resolve the form to clean up its keyboard/timer state before navigating.
  await page.evaluate(()=>{W.choose&&W.choose({a:'end'});});await pause(10);
  await page.evaluate(()=>{PEND={20:14500,25:700};D.p[20]=14400;sendresult({20:14400,21:14300},{20:14400,21:14300},true);});
  await fit(width+' named per-variable feedback');
  await page.evaluate(()=>{sdlgx();PEND={};const batch={},sent={};XIDS.forEach((id,i)=>{batch[id]=pdflt(id)??0;sent[id]=batch[id];if(i<12){D.p[id]=batch[id]+1;qput(id,batch[id]);}else D.p[id]=batch[id];});sendresult(batch,sent,true);});
  await fit(width+' large grouped restore report');
  const reportLayout=await page.evaluate(()=>{
   const box=document.querySelector('#sres .rb').getBoundingClientRect(),body=document.querySelector('#srsm'),actions=document.querySelector('#srsa').getBoundingClientRect();
   const values=[...document.querySelectorAll('.srvalues>span')].filter(e=>e.getClientRects().length);
   return {box:box.toJSON(),actions:actions.toJSON(),scrolls:body.scrollHeight>body.clientHeight,valuesFit:values.every(e=>e.scrollWidth<=e.clientWidth+1&&getComputedStyle(e).whiteSpace==='nowrap'),isolated:[...document.querySelectorAll('.srvalues bdi')].every(e=>getComputedStyle(e).direction==='ltr')};
  });
  check(reportLayout.box.top>=0&&reportLayout.box.bottom<=720&&reportLayout.actions.bottom<=720&&reportLayout.scrolls,width+' long report scrolls internally and keeps actions visible',reportLayout);
  check(reportLayout.valuesFit&&reportLayout.isolated,width+' report numbers and labels remain unbroken and directionally isolated',reportLayout);
  check(await page.locator('[data-group="different"]:visible').count()===1&&await page.locator('[data-group="confirmed"]:visible').count()===0,width+' restore report opens on unresolved settings only');
  await page.locator('[data-filter="confirmed"]').click();await fit(width+' filtered successful settings');
  check(await page.locator('[data-group="confirmed"]:visible .sritem').count()>0&&await page.locator('[data-group="different"]:visible').count()===0,width+' success filter shows matching settings without mixed unresolved rows');
  await page.locator('[data-filter="unknown"]').click();await fit(width+' empty unknown filter');


  await page.evaluate(()=>{PEND={30:7000,31:1000,32:1000};const batch={20:14400,21:14300,23:13500,24:12800,25:650,26:100,27:14800,28:150,29:6000};const rows=Object.entries(batch).map(([id,v],i)=>{if(i>=3)PEND[id]=v;return {id,back:i<3?v:i<6?v+1:null};});sendresult(batch,batch,true,[],rows);});
  for(const kind of ['different','unknown','queued','confirmed']){
   await page.locator('[data-filter="'+kind+'"]').click();await fit(width+' explained '+kind+' group');
   check(await page.locator('[data-group="'+kind+'"]:visible .sritem').count()===3,width+' '+kind+' shows three settings with plain-language help');
  }
  check(await page.locator('#srsa [onclick="sendRetry()"]').isVisible()&&await page.locator('#srsa [onclick="sendRecheck()"]').isVisible(),width+' real retry and recheck remain visible with longer explanations');
  await page.evaluate(()=>{sdlgx();PEND={};pbar();});

 }
 console.log(checks+' BROWSER LAYOUT CHECKS; '+failures+' FAILURE(S)');if(failures)process.exitCode=1;
 }finally{if(browser)await browser.close();server.kill();}
})().catch(e=>{console.error(e);process.exitCode=1;});
